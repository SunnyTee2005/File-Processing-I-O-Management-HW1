# Disk I/O 效能實驗：C Library (stdio) vs. System Call

> 中興大學資工系「File Processing and I/O Systems」課程 HW1（2026 春）
> 在 Linux 上用兩種 I/O 介面，量測 100 MB 檔案在 **循序 / 隨機 × 讀 / 寫 × 有無 fsync** 五種工作負載下的效能，並分析差異的來源。

---

## 這個實驗想回答的問題

同樣是「讀寫一個檔案」，呼叫 `fread()` 和呼叫 `read()` 到底差在哪？
資料從程式走到硬碟，中間經過了哪些緩衝層？每一層又在什麼情況下幫上忙、在什麼情況下變成負擔？

```
 ┌────────────────────────────┐
 │  User program               │
 │    fwrite() / fread()       │──┐  HW1_1 (stdio)
 │  ┌──────────────────────┐   │  │
 │  │ stdio buffer (user)  │◄──┼──┘  ← 第一層緩衝：C library 自己管
 │  └──────────┬───────────┘   │
 │             │ write() / read()  ◄── HW1_2 直接從這裡進入
 └─────────────┼───────────────┘
 ══════════════╪══════════════════  user / kernel 邊界（system call）
 ┌─────────────▼───────────────┐
 │  Page Cache (kernel, RAM)   │  ← 第二層緩衝：OS 管
 └─────────────┬───────────────┘
               │ fsync() 或 OS 自行決定回寫
 ┌─────────────▼───────────────┐
 │  Disk                       │
 └─────────────────────────────┘
```

- **HW1_1**：`fopen / fread / fwrite / fseek / fflush / fclose`（多一層 user-space buffer）
- **HW1_2**：`open / read / write / lseek / close`（每次呼叫都是一次 system call）

---

## 實驗設計

測試檔為 100 MB（`create_test_file.c` 產生），五種工作負載：

| # | Workload | 操作 | 次數 | fsync |
|---|---|---|---|---|
| 1 | Sequential Read | 從頭到尾每次讀 4 KB | 25,600 | — |
| 2 | Sequential Write | 從頭到尾每次寫 2 KB，覆寫整個檔案 | 51,200 | 全部寫完後 1 次 |
| 3 | Random Read | 隨機挑 4 KB 對齊的 offset，讀 4 KB | 50,000 | — |
| 4 | Random Buffered Write | 隨機挑 4 KB 對齊的 offset，寫 2 KB | 50,000 | 全部寫完後 1 次 |
| 5 | Random Synchronous Write | 同上 | 50,000 | **每次寫完都 fsync** |

幾個刻意的設計：

- **每次測試前清空 page cache**（`echo 3 > /proc/sys/vm/drop_caches`）。否則第二次讀取時 OS 會直接從 RAM 給資料，量到的是記憶體速度而不是磁碟 I/O。
- **計時終點放在 `fsync()` 之後**。`write()` 回傳只代表資料進了 page cache，不代表寫到磁碟；若在 fsync 前就停錶，量到的只是寫 RAM 的時間。
- **隨機 offset 對齊 4 KB**：`(rand() % (FILE_SIZE / 4096)) * 4096`。4 KB 是 Linux page 的大小，對齊後每次操作恰好落在一個 page 上。
- **寫入只寫 2 KB 到 4 KB 的 page 上**：這是 partial-page write。如果該 page 不在 cache 裡，kernel 必須先把整個 4 KB 讀進來、改掉其中 2 KB、再標記為 dirty（read-modify-write），所以「寫」其實隱含了「讀」。
- 每支測試用命令列參數選擇（`./HW1_1 3`），每次只跑一種 workload，避免前一個測試留下的 cache 影響下一個。

---

## 實作上踩過、想清楚的點

**1. stdio 版本要 `fflush()` 之後才能 `fsync()`**

```c
fflush(f1);          // stdio buffer (user space) → page cache (kernel)
fsync(fileno(f1));   // page cache → disk
```

`fsync()` 只認 file descriptor，只會把 *kernel* 裡的 dirty page 寫到磁碟；還留在 stdio buffer 裡的資料 kernel 根本看不到。所以 stdio 版本必須先 `fflush` 再 `fsync`，並用 `fileno()` 從 `FILE *` 取出底層的 fd。這也是兩層緩衝最直接的證據。

**2. 為什麼不直接開一個 100 MB 的陣列一次寫完？**

`char buffer[100 * 1024 * 1024]` 會配置在 stack 上，而 Linux 預設 stack 大小通常只有 8 MB，會直接 segmentation fault。所以改用 4 KB 的 buffer 分批寫（chunking），大小剛好等於一個 page。

**3. `FILE *` 和 fd 的關係**

fd 是 kernel 給的一個整數索引（指向該 process 的 open file table）；`FILE` 是 C library 定義的 struct，裡面包著一個 fd，加上一塊 user-space buffer 和讀寫位置等狀態。`fclose()` 會先 flush 這塊 buffer 再釋放 fd——但 flush 只到 page cache，**不保證**落到磁碟。

**4. `srand()` 只呼叫一次**

`rand()` 是由 seed 決定的偽隨機序列，在迴圈裡重複 `srand(time(NULL))` 會因為同一秒內 seed 相同而一直產生同一個 offset。

---

## 結果

環境：Linux（Ubuntu VM），每項測試前清空 page cache，單次量測。

| Workload | HW1_1 stdio | HW1_2 syscall | HW1_1 每次操作 | HW1_2 每次操作 |
|---|---:|---:|---:|---:|
| Sequential Read | 33,335 µs | 22,933 µs | 1.3 µs | 0.9 µs |
| Sequential Write | 76,543 µs | 169,280 µs | 1.5 µs | 3.3 µs |
| Random Read | 547,965 µs | 8,105,902 µs | 11.0 µs | 162.1 µs |
| Random Buffered Write | 455,187 µs | 436,982 µs | 9.1 µs | 8.7 µs |
| Random Synchronous Write | 9,384,983 µs | 8,960,671 µs | 187.7 µs | 179.2 µs |

原始輸出見 [`results.txt`](results.txt)。

---

## 分析

**Sequential Write：stdio 快約 2.2 倍** — 最能看出 user-space buffer 價值的一項。
HW1_2 每寫 2 KB 就是一次 `write()` system call，共 51,200 次 user/kernel 切換；HW1_1 的 `fwrite()` 先把資料累積在 stdio buffer（glibc 預設大小通常等於檔案系統 block size，4 KB），滿了才呼叫一次 `write()`，system call 次數大約減半。資料量一樣，差的就是 system call 的固定成本。

**Random Synchronous Write vs. Random Buffered Write：慢約 20 倍** — fsync 的代價。
同樣是 50,000 次 2 KB 隨機寫入，只差在 fsync 的次數（50,000 次 vs. 1 次）。每次 fsync 都要等裝置回報寫入完成，平均約 180 µs；而 buffered 版本讓 page cache 吸收所有寫入，最後一次 fsync 時 OS 可以合併、排序後再寫出。這項兩種介面幾乎一樣，因為每次都 `fflush + fsync`，stdio buffer 完全沒有發揮空間——瓶頸在磁碟，不在介面。

這也是 database 等系統要在「durability」和「throughput」之間取捨的原因：每筆都 fsync 最安全但最慢，所以才會有 group commit、WAL 這類設計。

**Sequential Read：差距不大**
兩者都約 3–4 GB/s，kernel 對循序讀取會做 readahead，大部分時間花在 page cache 到 user buffer 的複製上。syscall 版本略快，可能是少了一次 stdio buffer 的複製；但差距只有 10 ms、又是單次量測，不足以下定論。

---

## 數據中的疑點

**Random Read 的 15 倍差距，不太可能是 stdio 造成的。**

一開始的直覺解釋是「stdio 會預讀，所以隨機讀比較快」，但仔細看每次操作的時間：

- HW1_2：平均 **162 µs / 次**，符合實際讀取（虛擬）磁碟的延遲量級。
- HW1_1：平均 **11 µs / 次**，這是記憶體等級的速度，代表大部分讀取都命中了 page cache。

而 stdio 在 `fseek()` 到新位置時會丟掉原本的 buffer，隨機存取下它幾乎沒有預讀可以利用，system call 次數和 HW1_2 差不多。所以比較合理的解釋是：**HW1_1 那次量測時 page cache 沒有真正清乾淨**，比較的其實是「cache 命中」vs.「真的讀磁碟」，而不是 stdio vs. syscall。

同樣的疑點也出現在 **Random Buffered Write**：如果 cache 真的是冷的，每次 2 KB partial-page write 都要先把 page 讀進來，應該接近 random read 的速度，但兩個版本都只有約 9 µs / 次。

另一個值得注意的地方：50,000 次隨機存取分布在 25,600 個 block 上，期望會碰到的不同 block 只有約 25,600 × (1 − e^(−50000/25600)) ≈ **22,000 個**，其餘約 28,000 次是在同一次測試中重複存取已經進入 cache 的 page。所以就算一開始 cache 是冷的，random 測試後半段也會越來越「熱」。

如果重做，我會：

1. 清 cache 前先 `sync`（`drop_caches` 只會丟掉 clean page，dirty page 不會被清掉）。
2. 每項跑 5 次以上取中位數，並交錯執行 HW1_1 / HW1_2，排除系統狀態漂移。
3. 在實體機上跑——VM 的虛擬磁碟背後還有 host OS 的 page cache，guest 裡 drop_caches 清不到那一層，絕對數值會偏樂觀。
4. 用 `strace -c` 實際數 system call 次數，驗證上面「stdio 讓 write() 次數減半」的推論。
5. 用 `O_DIRECT` 繞過 page cache 做對照組。

---

## 如何重現

```bash
# 編譯
gcc -O2 -o create_test_file create_test_file.c
gcc -O2 -o HW1_1 HW1_1.c
gcc -O2 -o HW1_2 HW1_2.c

# 產生 100 MB 測試檔 test.txt
./create_test_file

# 每項測試前清空 page cache（需要 root）
for prog in HW1_1 HW1_2; do
  for t in 1 2 3 4 5; do
    sync
    sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
    ./$prog $t
  done
done
```

測試編號：`1` Seq Read、`2` Seq Write、`3` Random Read、`4` Random Buffered Write、`5` Random Synchronous Write。

---

## 檔案說明

| 檔案 | 說明 |
|---|---|
| `HW1_1.c` | C library (stdio) 版本的五種 workload |
| `HW1_2.c` | System call 版本的五種 workload |
| `create_test_file.c` | 產生 100 MB 測試檔 |
| `results.txt` | 量測結果原始輸出 |
| `HW1_1_note.txt` | 做作業時的筆記：FILE vs. fd、fclose vs. fsync、drop_caches、stack overflow |
| `concepts_summary.md` | 觀念整理：I/O stack、page cache、open() flags、4 KB 對齊、mmap 概念 |
| `HW1.pdf` / `HW1.pptx` | 課程作業說明 |

原作業另有 mmap 版本（HW1_3）與書面報告（HW1_4），本 repo 收錄的是 stdio 與 system call 兩種介面的實作與量測。

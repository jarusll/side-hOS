target remote localhost:1234
symbol-file build/kernel.elf
b kmain

define log_ram
python
import gdb
import os

names = {
    0: "USABLE",
    1: "RESERVED",
    2: "ACPI_RECLAIM",
    3: "ACPI_NVS",
    4: "BAD",
    5: "BOOTLOADER",
    6: "EXEC/MODULE",
    7: "FRAMEBUFFER",
    8: "RES_MAPPED"
}

limit = int(gdb.parse_and_eval("memmap_request.response.entry_count"))
entries = gdb.parse_and_eval("memmap_request.response.entries")

segments = []

for i in range(limit):
    entry = entries[i].dereference()
    base = int(entry['base'])
    size = int(entry['length'])
    typ  = int(entry['type'])
    segment_end = base + size
    segments.append((names[typ], base, size))
    segments.sort(key = lambda x: x[1])

string = ""
for name, seg_start, seg_size in segments:
    string += f"{name}, {seg_start}, {seg_size}\n"

os.makedirs("artifacts", exist_ok=True)
with open('artifacts/segments.txt', 'w') as f:
    f.write(string)
end
end

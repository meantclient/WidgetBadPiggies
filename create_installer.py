import os
import struct
import subprocess
import zipfile
import shutil

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PAYLOAD_BIN = os.path.join(BASE_DIR, "payload.bin")
DESKTOP_EXE = os.path.join(BASE_DIR, "BadPiggiesSetup.exe")
DESKTOP_ZIP_SETUP = os.path.join(BASE_DIR, "BadPiggiesSetup.zip")
DESKTOP_ZIP_PORTABLE = os.path.join(BASE_DIR, "BadPiggiesWidget.zip")

files_to_pack = [
    "BadPiggiesDotaWidget.exe",
    "config.ini",
    "StartWidget.bat",
    "ИНСТРУКЦИЯ_ДЛЯ_ДРУЗЕЙ.txt",
    "libvosk.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
]

all_entries = []

for f in files_to_pack:
    p = os.path.join(BASE_DIR, f)
    if os.path.exists(p):
        all_entries.append((p, f))

assets_dir = os.path.join(BASE_DIR, "assets")
for root, dirs, files in os.walk(assets_dir):
    for f in files:
        full_p = os.path.join(root, f)
        rel_p = os.path.relpath(full_p, BASE_DIR)
        all_entries.append((full_p, rel_p))

model_dir = os.path.join(BASE_DIR, "model")
if os.path.exists(model_dir):
    for root, dirs, files in os.walk(model_dir):
        for f in files:
            full_p = os.path.join(root, f)
            rel_p = os.path.relpath(full_p, BASE_DIR)
            all_entries.append((full_p, rel_p))

print(f"Total files to bundle: {len(all_entries)}")

with open(PAYLOAD_BIN, "wb") as out:
    out.write(b"BPKG")
    out.write(struct.pack("<I", len(all_entries)))
    for full_p, rel_p in all_entries:
        rel_bytes = rel_p.replace('/', '\\').encode("utf-8")
        with open(full_p, "rb") as f_in:
            data = f_in.read()
        out.write(struct.pack("<H", len(rel_bytes)))
        out.write(rel_bytes)
        out.write(struct.pack("<I", len(data)))
        out.write(data)

print(f"Created payload.bin ({os.path.getsize(PAYLOAD_BIN)} bytes)")

print("\nBuilding BadPiggiesSetup.exe...")
os.system("build_installer.bat")

if os.path.exists(DESKTOP_EXE):
    size = os.path.getsize(DESKTOP_EXE)
    print(f"\nInstaller created: {DESKTOP_EXE} ({size} bytes)")
    
    # Also create BadPiggiesSetup.zip so browsers never show warnings
    with zipfile.ZipFile(DESKTOP_ZIP_SETUP, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(DESKTOP_EXE, "BadPiggiesSetup.exe")
    print(f"Created BadPiggiesSetup.zip ({os.path.getsize(DESKTOP_ZIP_SETUP)} bytes)")
else:
    print("ERROR: BadPiggiesSetup.exe not found!")

# Portable zip
with zipfile.ZipFile(DESKTOP_ZIP_PORTABLE, 'w', zipfile.ZIP_DEFLATED) as z:
    for full_p, rel_p in all_entries:
        z.write(full_p, rel_p)
print(f"Updated portable BadPiggiesWidget.zip ({os.path.getsize(DESKTOP_ZIP_PORTABLE)} bytes)")

# if os.path.exists(PAYLOAD_BIN):
#     os.remove(PAYLOAD_BIN)
print("Payload left intact for inspection.")
print("Done!")

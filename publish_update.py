import os
import sys
import json
import re
import subprocess
import shutil

BASE_DIR = r"c:\Users\Максим\Desktop\WidgetBadPiges"
CONFIG_H = os.path.join(BASE_DIR, "src", "config.h")
CONFIG_INI = os.path.join(BASE_DIR, "config.ini")
VERSION_JSON = os.path.join(BASE_DIR, "version.json")
INSTALLER_RC = os.path.join(BASE_DIR, "installer", "installer.rc")

def get_current_version():
    try:
        with open(CONFIG_INI, "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("app_version="):
                    return line.strip().split("=")[1]
    except:
        pass
    return "2.4"

def update_files_version(new_ver, changelog):
    print(f"\n[1/5] Updating version references to v{new_ver}...")

    # 1. Update config.ini
    if os.path.exists(CONFIG_INI):
        with open(CONFIG_INI, "r", encoding="utf-8") as f:
            ini_text = f.read()
        ini_text = re.sub(r"app_version=.*", f"app_version={new_ver}", ini_text)
        with open(CONFIG_INI, "w", encoding="utf-8") as f:
            f.write(ini_text)
        print("  -> Updated config.ini")

    # 2. Update src/config.h
    if os.path.exists(CONFIG_H):
        with open(CONFIG_H, "r", encoding="utf-8") as f:
            h_text = f.read()
        h_text = re.sub(r'std::string appVersion = ".*";', f'std::string appVersion = "{new_ver}";', h_text)
        with open(CONFIG_H, "w", encoding="utf-8") as f:
            f.write(h_text)
        print("  -> Updated src/config.h")

    # 3. Update version.json
    v_data = {
        "version": new_ver,
        "download_url": "https://raw.githubusercontent.com/meantclient/WidgetBadPiggies/main/BadPiggiesSetup.exe",
        "changelog": changelog
    }
    with open(VERSION_JSON, "w", encoding="utf-8") as f:
        json.dump(v_data, f, ensure_ascii=False, indent=2)
    print("  -> Updated version.json")

def build_project():
    print("\n[2/5] Compiling BadPiggiesDotaWidget.exe...")
    res = subprocess.run(["cmd.exe", "/c", "build.bat"], cwd=BASE_DIR, capture_output=True, text=True)
    if res.returncode != 0:
        print("[ERROR] Build failed!")
        print(res.stdout)
        print(res.stderr)
        return False
    print("  -> Widget built successfully!")

    print("\n[3/5] Building BadPiggiesSetup.exe and zip packages...")
    res2 = subprocess.run([sys.executable, "create_installer.py"], cwd=BASE_DIR, capture_output=True, text=True)
    if res2.returncode != 0:
        print("[ERROR] Installer build failed!")
        print(res2.stdout)
        print(res2.stderr)
        return False
    print("  -> Installer and ZIP packages created on Desktop!")
    
    # Copy BadPiggiesSetup.exe to local folder for GitHub commit
    desktop_exe = r"C:\Users\Максим\Desktop\BadPiggiesSetup.exe"
    local_exe = os.path.join(BASE_DIR, "BadPiggiesSetup.exe")
    if os.path.exists(desktop_exe):
        shutil.copyfile(desktop_exe, local_exe)
        print("  -> Copied BadPiggiesSetup.exe to repository root")

    return True

def handle_git(new_ver, changelog):
    print("\n[4/5] Checking Git repository...")
    git_dir = os.path.join(BASE_DIR, ".git")
    if not os.path.exists(git_dir):
        print("  -> Initializing local git repository...")
        subprocess.run(["git", "init"], cwd=BASE_DIR, capture_output=True)

    subprocess.run(["git", "add", "."], cwd=BASE_DIR, capture_output=True)
    msg = f"Release v{new_ver}: {changelog}"
    commit_res = subprocess.run(["git", "commit", "-m", msg], cwd=BASE_DIR, capture_output=True, text=True)
    print("  -> Committed release changes to Git.")

    # Check if remote exists
    rem_res = subprocess.run(["git", "remote", "-v"], cwd=BASE_DIR, capture_output=True, text=True)
    if "origin" in rem_res.stdout:
        print("  -> Pushing release to GitHub...")
        push_res = subprocess.run(["git", "push", "-u", "origin", "main"], cwd=BASE_DIR, capture_output=True, text=True)
        if push_res.returncode == 0:
            print("  -> SUCCESS: Pushed to GitHub! Auto-update is now live for all users!")
        else:
            print("  -> Note: Push returned code", push_res.returncode, "(run 'git push' when ready).")
    else:
        print("  -> Note: Git remote 'origin' is not set yet.")
        print("     To connect GitHub:")
        print("     git remote add origin https://github.com/meantclient/WidgetBadPiggies.git")
        print("     git push -u origin main")

def main():
    cur_v = get_current_version()
    print("========================================================")
    print(f"   BAD PIGGIES WIDGET — ПУБЛИКАЦИЯ ОБНОВЛЕНИЯ")
    print(f"   Текущая версия: v{cur_v}")
    print("========================================================")

    if len(sys.argv) >= 2:
        new_ver = sys.argv[1].strip()
    else:
        new_ver = input(f"Введите номер новой версии (например 2.5): ").strip()
        if not new_ver:
            new_ver = "2.5"

    if len(sys.argv) >= 3:
        changelog = " ".join(sys.argv[2:]).strip()
    else:
        changelog = input("Что нового в этой версии (changelog): ").strip()
        if not changelog:
            changelog = f"Обновление v{new_ver}: улучшена стабильность и добавлены новые фразы."

    update_files_version(new_ver, changelog)
    if build_project():
        handle_git(new_ver, changelog)
        print("\n========================================================")
        print(f"  ГОТОВО! ВЕРСИЯ v{new_ver} УСПЕШНО СОБРАНА!")
        print("========================================================")
        print(f"1. На Рабочем столе обновлены файлы:")
        print(f"   - BadPiggiesSetup.exe")
        print(f"   - BadPiggiesSetup.zip")
        print(f"   - BadPiggiesWidget.zip")
        print(f"2. Автообновление:")
        print(f"   Когда файлы загружены на GitHub, у всех друзей при")
        print(f"   следующем запуске виджет сам скачает и поставит v{new_ver}!")
        print("========================================================\n")

if __name__ == "__main__":
    main()

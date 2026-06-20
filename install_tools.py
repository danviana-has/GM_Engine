import os
import urllib.request
import zipfile
import io
import shutil
import json

TOOLS_DIR = "tools"

def get_latest_mingw_url():
    print("Querying GitHub API for latest WinLibs MinGW release...")
    url = "https://api.github.com/repos/brechtsanders/winlibs_mingw/releases/latest"
    try:
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
        )
        with urllib.request.urlopen(req) as response:
            data = json.loads(response.read().decode('utf-8'))
            
        for asset in data.get("assets", []):
            name = asset.get("name", "")
            # We want: 64-bit, posix, seh, gcc, ucrt or msvcrt, zip format
            if "x86_64" in name and "posix" in name and "seh" in name and name.endswith(".zip") and "llvm" not in name:
                print(f"Found latest matching MinGW asset: {name}")
                return asset.get("browser_download_url")
    except Exception as e:
        print(f"Error querying GitHub API: {e}")
    
    # Fallback to a known valid version if API query fails
    print("Using fallback MinGW URL...")
    return "https://github.com/brechtsanders/winlibs_mingw/releases/download/14.1.0posix-18.1.8-12.0.0-ucrt-r1/winlibs-x86_64-posix-seh-gcc-14.1.0-mingw-w64ucrt-r1.zip"

def download_and_extract(url, name):
    print(f"Downloading {name} from {url}...")
    try:
        os.makedirs(TOOLS_DIR, exist_ok=True)
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
        )
        with urllib.request.urlopen(req) as response:
            zip_data = response.read()
        
        print(f"Extracting {name}...")
        with zipfile.ZipFile(io.BytesIO(zip_data)) as zip_ref:
            # Extract directly to tools directory
            zip_ref.extractall(TOOLS_DIR)
        print(f"{name} extracted successfully!")
    except Exception as e:
        print(f"Error installing {name}: {e}")

def main():
    # 1. Download portable MinGW-w64 dynamically
    mingw_url = get_latest_mingw_url()
    download_and_extract(mingw_url, "MinGW-w64")

    # 2. Download portable CMake (if not already downloaded)
    cmake_path = os.path.join(TOOLS_DIR, "cmake-3.29.3-windows-x86_64")
    if not os.path.exists(cmake_path) and not any("cmake" in f.lower() for f in os.listdir(TOOLS_DIR) if os.path.isdir(os.path.join(TOOLS_DIR, f))):
        cmake_url = "https://github.com/Kitware/CMake/releases/download/v3.29.3/cmake-3.29.3-windows-x86_64.zip"
        download_and_extract(cmake_url, "CMake")
    else:
        print("CMake already installed.")

    print("\nTools setup complete!")

if __name__ == "__main__":
    main()

import os
import urllib.request
import zipfile
import io
import shutil

THIRDPARTY_DIR = "thirdparty"

def download_and_extract_zip(url, target_subdir, rename_folder=None):
    print(f"Downloading {url}...")
    try:
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
        )
        with urllib.request.urlopen(req) as response:
            zip_data = response.read()
        
        print("Extracting...")
        with zipfile.ZipFile(io.BytesIO(zip_data)) as zip_ref:
            temp_extract = os.path.join(THIRDPARTY_DIR, "temp_extract")
            zip_ref.extractall(temp_extract)
            
            extracted_folders = [f for f in os.listdir(temp_extract) if os.path.isdir(os.path.join(temp_extract, f))]
            if extracted_folders:
                src_folder = os.path.join(temp_extract, extracted_folders[0])
                dest_folder = os.path.join(THIRDPARTY_DIR, rename_folder if rename_folder else target_subdir)
                
                if os.path.exists(dest_folder):
                    shutil.rmtree(dest_folder)
                
                shutil.move(src_folder, dest_folder)
                print(f"Saved to {dest_folder}")
            
            shutil.rmtree(temp_extract)
    except Exception as e:
        print(f"Error downloading/extracting {url}: {e}")

def download_file(url, target_path):
    print(f"Downloading {url} to {target_path}...")
    try:
        os.makedirs(os.path.dirname(target_path), exist_ok=True)
        req = urllib.request.Request(
            url, 
            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
        )
        with urllib.request.urlopen(req) as response:
            with open(target_path, 'wb') as out_file:
                out_file.write(response.read())
        print(f"Saved to {target_path}")
    except Exception as e:
        print(f"Error downloading {url}: {e}")

def main():
    if not os.path.exists(THIRDPARTY_DIR):
        os.makedirs(THIRDPARTY_DIR)

    # 1. GLFW (Precompiled Windows 64-bit Binaries)
    download_and_extract_zip(
        "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip", 
        "glfw-3.4", 
        "glfw"
    )

    # 2. ImGui (docking branch is essential for professional docking layouts)
    download_and_extract_zip(
        "https://github.com/ocornut/imgui/archive/refs/heads/docking.zip", 
        "imgui-docking", 
        "imgui"
    )

    # 3. GLM
    download_and_extract_zip(
        "https://github.com/g-truc/glm/archive/refs/tags/0.9.9.8.zip", 
        "glm-0.9.9.8", 
        "glm"
    )

    # 4. JSON
    download_file(
        "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp",
        os.path.join(THIRDPARTY_DIR, "json", "nlohmann", "json.hpp")
    )

    # 5. Glad (OpenGL 3.3 Core Loader files from JoeyDeVries/LearnOpenGL repository)
    download_file(
        "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/src/glad.c",
        os.path.join(THIRDPARTY_DIR, "glad", "src", "glad.c")
    )
    download_file(
        "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/includes/glad/glad.h",
        os.path.join(THIRDPARTY_DIR, "glad", "include", "glad", "glad.h")
    )
    download_file(
        "https://raw.githubusercontent.com/JoeyDeVries/LearnOpenGL/master/includes/KHR/khrplatform.h",
        os.path.join(THIRDPARTY_DIR, "glad", "include", "KHR", "khrplatform.h")
    )
    # 6. ImGuizmo
    download_file(
        "https://raw.githubusercontent.com/CedricGuillemet/ImGuizmo/master/src/ImGuizmo.h",
        os.path.join(THIRDPARTY_DIR, "imguizmo", "ImGuizmo.h")
    )
    download_file(
        "https://raw.githubusercontent.com/CedricGuillemet/ImGuizmo/master/src/ImGuizmo.cpp",
        os.path.join(THIRDPARTY_DIR, "imguizmo", "ImGuizmo.cpp")
    )

    print("\nAll dependencies downloaded successfully!")

if __name__ == "__main__":
    main()

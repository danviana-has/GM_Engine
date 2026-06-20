import os
import shutil
import subprocess
import time

EXE_PATH = "build/GMEngine.exe"
TARGET_PLAY_PATH = "build/TestCompiledGame.exe"

def main():
    print("Starting automated verification...")
    
    if not os.path.exists(EXE_PATH):
        print(f"Error: {EXE_PATH} not found!")
        return False
        
    print(f"Found compiled engine at {EXE_PATH} ({os.path.getsize(EXE_PATH)} bytes)")
    
    # 1. Create a mock scene JSON matching GM Engine structure
    mock_scene = {
        "ambientColor": [0.4, 0.4, 0.4],
        "sunDirection": [0.5, -1.0, 0.3],
        "skyColor": [0.3, 0.6, 0.9],
        "workspace": [
            {
                "id": 1,
                "name": "Baseplate",
                "shape": "Block",
                "material": "Plastic",
                "position": [0.0, -1.0, 0.0],
                "size": [100.0, 2.0, 100.0],
                "rotation": [0.0, 0.0, 0.0],
                "color": [0.3, 0.8, 0.3, 1.0],
                "transparency": 0.0,
                "reflectance": 0.1,
                "anchored": True,
                "canCollide": True
            },
            {
                "id": 2,
                "name": "PhysicsCube",
                "shape": "Block",
                "material": "Metal",
                "position": [0.0, 10.0, 0.0],
                "size": [3.0, 3.0, 3.0],
                "rotation": [0.0, 0.0, 0.0],
                "color": [0.9, 0.1, 0.1, 1.0],
                "transparency": 0.0,
                "reflectance": 0.8,
                "anchored": False, # Unanchored so it falls!
                "canCollide": True
            }
        ]
    }
    
    import json
    scene_json_str = json.dumps(mock_scene, indent=4)
    
    # 2. Duplicate executable
    print(f"Copying {EXE_PATH} to {TARGET_PLAY_PATH}...")
    shutil.copyfile(EXE_PATH, TARGET_PLAY_PATH)
    
    # 3. Append scene data
    print("Appending scene payload...")
    with open(TARGET_PLAY_PATH, "ab") as f:
        f.write(b"//GM_SCENE_START//")
        f.write(scene_json_str.encode("utf-8"))
        
    print(f"Payload appended! Total game size: {os.path.getsize(TARGET_PLAY_PATH)} bytes")
    
    # 4. Run the compiled executable to verify it works (should open window and run playmode)
    print("Launching test game (3 seconds run window)...")
    try:
        # Run with a timeout. If it runs for 3 seconds without crashing and we have to kill it, it works!
        process = subprocess.Popen([TARGET_PLAY_PATH], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        time.sleep(3.0)
        
        # Check if process is still running
        poll = process.poll()
        if poll is None:
            print("SUCCESS: Game launched and remained stable in playmode!")
            process.terminate()
            process.wait()
            return True
        else:
            stdout, stderr = process.communicate()
            print(f"FAILURE: Game exited prematurely with code {poll}!")
            print(f"Stdout: {stdout.decode()}")
            print(f"Stderr: {stderr.decode()}")
            return False
            
    except Exception as e:
        print(f"Error executing compiled game: {e}")
        return False

if __name__ == "__main__":
    success = main()
    if success:
        print("\nVerification Passed Successfully!")
    else:
        print("\nVerification Failed!")

import subprocess
import sys


def run(args):
    print("+", " ".join(args))
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sys.stdout.write(p.stdout)
    if p.returncode != 0:
        raise SystemExit(p.returncode)


run(["adb", "shell", "killall", "ipc_firmware"])
run([
    "adb",
    "shell",
    "sh",
    "-c",
    "cd /mnt/UDISK || exit 90; chmod +x ipc_firmware || exit 91; export LD_LIBRARY_PATH=/mnt/UDISK:/lib:/usr/lib; nohup /mnt/UDISK/ipc_firmware --enable-odet --odet-model /mnt/UDISK/Facedet_480_288_nv12.nb >/tmp/ipc_face.log 2>&1 </dev/null &",
])
run(["adb", "shell", "ps"])
run(["adb", "shell", "ls", "-l", "/tmp/ipc_face.log"])
run(["adb", "shell", "tail", "-n", "80", "/tmp/ipc_face.log"])

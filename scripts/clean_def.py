import os

def clean_def_file(filepath):
    if not os.path.exists(filepath):
        print(f"File not found: {filepath}")
        return
    with open(filepath, "rb") as f:
        data = f.read()
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    data = data.replace(b"\x00", b"")
    text = data.decode("utf-8", errors="ignore")
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    cleaned = "\r\n".join(lines)
    with open(filepath, "wb") as f:
        f.write(cleaned.encode("utf-8"))
    print(f"Successfully cleaned {filepath}")

if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target = os.path.join(base_dir, "KxNt", "KxNt.def")
    clean_def_file(target)

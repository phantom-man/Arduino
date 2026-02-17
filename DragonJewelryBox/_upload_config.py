"""Upload a config file to Qidi Q2 printer via Moonraker API."""
import urllib.request
import json
import sys

PRINTER = "http://192.168.0.115"

def upload_config(local_path, remote_name=None):
    if remote_name is None:
        import os
        remote_name = os.path.basename(local_path)

    with open(local_path, "rb") as f:
        file_data = f.read()

    boundary = "----PythonUploadBoundary9876"
    parts = []

    # root=config
    parts.append(f"--{boundary}\r\n".encode())
    parts.append(b'Content-Disposition: form-data; name="root"\r\n\r\n')
    parts.append(b"config\r\n")

    # file
    parts.append(f"--{boundary}\r\n".encode())
    parts.append(f'Content-Disposition: form-data; name="file"; filename="{remote_name}"\r\n'.encode())
    parts.append(b"Content-Type: application/octet-stream\r\n\r\n")
    parts.append(file_data)
    parts.append(b"\r\n")

    # end
    parts.append(f"--{boundary}--\r\n".encode())

    body = b"".join(parts)

    req = urllib.request.Request(
        f"{PRINTER}/server/files/upload",
        data=body,
        method="POST",
    )
    req.add_header("Content-Type", f"multipart/form-data; boundary={boundary}")

    resp = urllib.request.urlopen(req)
    result = json.loads(resp.read())
    print(f"Upload '{remote_name}': {json.dumps(result)}")
    return result


if __name__ == "__main__":
    local = sys.argv[1]
    remote = sys.argv[2] if len(sys.argv) > 2 else None
    upload_config(local, remote)

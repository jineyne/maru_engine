#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Third-party bootstrapper:
- Downloads vendored third-parties
- Verifies checksums via manifest.lock (write-on-first-run)
- Extracts to src/thirdparty/<name>
- Idempotent & cache-aware

Usage:
  python3 bootstrap.py
  python3 bootstrap.py --force      # re-download & re-extract
  python3 bootstrap.py --clean      # remove extracted dirs and caches
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import os
import shutil
import sys
import tarfile
import zipfile
from pathlib import Path
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError

# ---------- Config ----------
ROOT = Path(__file__).resolve().parent
SRC_TP = ROOT / "src" / "thirdparty"
CACHE = ROOT / ".bootstrap_cache"
LOCK  = ROOT / "manifest.lock"

# Pin versions here
PACKAGES = [
    {
        "name": "glfw",
        "version": "3.4",  # pin; update here to upgrade
        "url": "https://github.com/glfw/glfw/archive/refs/tags/{ver}.tar.gz",
        "dst_dir": SRC_TP / "glfw",
        "filename": "glfw-{ver}.tar.gz",
        "inner_top": "glfw-{ver}",
    },
    {
        "name": "cglm",
        "version": "0.9.2",
        "url": "https://github.com/recp/cglm/archive/refs/tags/v{ver}.tar.gz",
        "dst_dir": SRC_TP / "cglm",
        "filename": "cglm-{ver}.tar.gz",
        "inner_top": "cglm-{ver}",
    },
    # 추가 패키지는 여기에 같은 형식으로 append
]

# ---------- Helpers ----------
def sha256_bytestr(b: bytes) -> str:
    h = hashlib.sha256()
    h.update(b)
    return h.hexdigest()

def sha256_file(p: Path, chunk: int = 1024 * 1024) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        while True:
            buf = f.read(chunk)
            if not buf:
                break
            h.update(buf)
    return h.hexdigest()

def human(n: int) -> str:
    for unit in ("B","KB","MB","GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.1f}{unit}"
        n /= 1024.0
    return f"{n:.1f}GB"

def ensure_dirs():
    SRC_TP.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)

def load_lock() -> dict:
    if LOCK.exists():
        with LOCK.open("r", encoding="utf-8") as f:
            return json.load(f)
    return {"packages": {}}

def save_lock(data: dict):
    tmp = LOCK.with_suffix(".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        f.write("\n")
    tmp.replace(LOCK)

def download(url: str, dst: Path):
    # simple streaming download with progress
    req = Request(url, headers={"User-Agent": "bootstrap.py"})
    try:
        with urlopen(req) as r:
            total = int(r.headers.get("Content-Length", "0"))
            read = 0
            buf = io.BytesIO()
            while True:
                chunk = r.read(1024 * 64)
                if not chunk:
                    break
                buf.write(chunk)
                read += len(chunk)
                if total:
                    pct = int(read * 100 / total)
                    sys.stdout.write(f"\r  - downloading {dst.name}: {pct}% ({human(read)}/{human(total)})")
                else:
                    sys.stdout.write(f"\r  - downloading {dst.name}: {human(read)}")
                sys.stdout.flush()
            sys.stdout.write("\n")
        dst.write_bytes(buf.getvalue())
    except HTTPError as e:
        raise RuntimeError(f"HTTP error {e.code} while fetching {url}") from e
    except URLError as e:
        raise RuntimeError(f"URL error while fetching {url}: {e.reason}") from e

def extract_archive(archive: Path, dst_dir: Path, inner_top: str | None):
    if dst_dir.exists():
        shutil.rmtree(dst_dir)
    dst_dir.parent.mkdir(parents=True, exist_ok=True)

    if archive.suffixes[-2:] == [".tar", ".gz"] or archive.suffix == ".tgz":
        with tarfile.open(archive, "r:gz") as tf:
            tf.extractall(dst_dir.parent)
    elif archive.suffix == ".zip":
        with zipfile.ZipFile(archive, "r") as zf:
            zf.extractall(dst_dir.parent)
    else:
        raise RuntimeError(f"Unsupported archive type: {archive.name}")

    # Move/normalize to dst_dir
    if inner_top:
        inner = dst_dir.parent / inner_top
        if inner.exists():
            if dst_dir.exists():
                shutil.rmtree(dst_dir)
            inner.rename(dst_dir)
        else:
            # If the expected inner dir name doesn't exist, try to detect the single top folder
            candidates = [p for p in dst_dir.parent.iterdir() if p.is_dir() and p.name.startswith(dst_dir.name)]
            if len(candidates) == 1:
                candidates[0].rename(dst_dir)
            else:
                raise RuntimeError(f"Could not find extracted top directory for {archive.name}")
    dst_dir.mkdir(parents=True, exist_ok=True)

def clean(paths: list[Path]):
    for p in paths:
        if p.exists():
            if p.is_dir():
                shutil.rmtree(p)
            else:
                p.unlink()

# ---------- Core ----------
def process_package(pkg: dict, force: bool, lock: dict):
    name   = pkg["name"]
    ver    = pkg["version"]
    url    = pkg["url"].format(ver=ver)
    fname  = pkg["filename"].format(ver=ver)
    inner  = pkg.get("inner_top")
    dst    = Path(pkg["dst_dir"])
    cachef = CACHE / fname

    print(f"[{name} {ver}]")
    meta_key = f"{name}@{ver}"
    lock_pkg = lock["packages"].get(meta_key, {})

    # Download (use cache unless force)
    if force or not cachef.exists():
        print("  - fetching:", url)
        download(url, cachef)
    else:
        print("  - using cached:", cachef.name)

    # Compute sha256
    sha = sha256_file(cachef)
    print("  - sha256:", sha)

    # Verify against lock if present
    expected = lock_pkg.get("sha256")
    if expected:
        if expected != sha:
            raise RuntimeError(
                f"Checksum mismatch for {meta_key}\n  expected: {expected}\n  actual:   {sha}\n  file:     {cachef}"
            )
        else:
            print("  - checksum OK (manifest.lock)")

    # Extract (always if force; otherwise only if dst missing)
    if force or not dst.exists():
        print("  - extracting to:", dst)
        extract_archive(cachef, dst, inner_top=inner.format(ver=ver) if inner else None)
    else:
        print("  - already extracted:", dst)

    # Update lock (write-on-first-run or if missing)
    lock["packages"][meta_key] = {
        "name": name,
        "version": ver,
        "url": url,
        "archive": str(cachef.relative_to(ROOT)),
        "sha256": sha,
        "destination": str(dst.relative_to(ROOT)),
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="Re-download and re-extract packages")
    ap.add_argument("--clean", action="store_true", help="Remove extracted thirdparties and caches")
    args = ap.parse_args()

    if args.clean:
        print("Cleaning thirdparty and cache...")
        clean([SRC_TP, CACHE])
        print("Done.")
        return

    ensure_dirs()
    lock = load_lock()

    for pkg in PACKAGES:
        process_package(pkg, force=args.force, lock=lock)

    save_lock(lock)
    print("\nAll done ✅")
    print(f"- thirdparty root : {SRC_TP}")
    print(f"- cache           : {CACHE}")
    print(f"- manifest.lock   : {LOCK}")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nAborted.")
        sys.exit(130)
    except Exception as e:
        print(f"\nERROR: {e}", file=sys.stderr)
        sys.exit(1)

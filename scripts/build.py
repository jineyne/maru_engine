#!/usr/bin/env python3
# init/configure/build/install CLI skeleton
import argparse, subprocess, sys


parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(dest="cmd")


p_init = subparsers.add_parser("init")
p_init.add_argument("--name", required=True)


p_cfg = subparsers.add_parser("configure")
p_cfg.add_argument("--platform", choices=["windows","android","ios"], required=True)


p_build = subparsers.add_parser("build")
p_build.add_argument("--config", choices=["Debug","Release"], default="Debug")


p_inst = subparsers.add_parser("install")
p_inst.add_argument("--dest", default="install")


args = parser.parse_args()
print(f"build.py skeleton -> {args}")
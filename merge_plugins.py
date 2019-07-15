#!/usr/bin/env python
#
# Copyright 2019 DreamWorks Animation L.L.C.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import print_function

import argparse
import distutils.spawn
import os
import shutil
import subprocess

AVAILABLE_PLUGINS = {
    "nuke": {
        "about": "Plugins for Nuke10+",
        "files": [
            "third_party/nuke",
            "cmake",
            "CMakeLists.txt",
            "build_scripts"
        ]
    },
    "houdini_hydra": {
        "about": "Hydra extension for Pixar's Houdini plugin",
        "files": [
            "third_party/houdini"
        ]
    },
}

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
USD_DIR = None
HAS_MERGE = distutils.spawn.find_executable("merge") is not None


def make_parent_dirs(path):
    """Create all parent dirs for a path if they don't exists

    Args:
        path (str): File or directory to test for parent directories
    """
    out_dir = os.path.dirname(path)
    if not os.path.exists(out_dir):
        os.makedirs(out_dir)


def merge_dir(dir_path):
    """Merge a single directory to the equivalent in the USD repo

    Args:
        dir_path (str): Relative path of directory to merge

    """
    in_dir = os.path.join(THIS_DIR, dir_path)
    out_dir = os.path.join(USD_DIR, dir_path)

    if not os.path.exists(out_dir):
        make_parent_dirs(out_dir)
        print("Copying:    {}".format(dir_path))
        shutil.copytree(in_dir, out_dir)
        return

    for dir_item in os.listdir(in_dir):
        item_path = os.path.join(THIS_DIR, dir_path, dir_item)
        if os.path.isfile(item_path):
            merge_file(os.path.join(dir_path, dir_item))
        elif os.path.isdir(item_path):
            merge_dir(os.path.join(dir_path, dir_item))
        else:
            raise RuntimeError("Don't know what to do with {}".format(dir_item))


def merge_file(file_path):
    """Merge a single file to the equivalent in the USD repo

    Args:
        file_path (str): Relative path of file to merge

    """
    in_file = os.path.join(THIS_DIR, file_path)
    out_file = os.path.join(USD_DIR, file_path)

    if not os.path.exists(out_file) or not HAS_MERGE:
        make_parent_dirs(out_file)
        shutil.copyfile(in_file, out_file)
        print("Copying:    {}".format(file_path))
    else:
        merge_cmd = "merge {out_file} {out_file} {in_file}".format(
            in_file=in_file,
            out_file=out_file
        )

        print("Merging:    {}".format(file_path))
        result = subprocess.call(merge_cmd, shell=True)
        if result:
            print("Failed merge: {}".format(file_path))


def merge_plugin(plugin_name):
    """Merge a single USD plugin into the USD repo

    Args:
        plugin_name (str): Name of plugin to merge

    """
    for relative_file in AVAILABLE_PLUGINS[plugin_name]["files"]:
        orig_file = os.path.join(THIS_DIR, relative_file)
        if not os.path.exists(orig_file):
            raise RuntimeError("dwa_usd_plugins file {} does not exist".format(orig_file))

        if os.path.isdir(orig_file):
            merge_dir(relative_file)
        elif os.path.isfile(orig_file):
            merge_file(relative_file)
        else:
            raise RuntimeError("Unknown file situation {}".format(orig_file))


def validate_usd_dir(usd_dir):
    """Make sure the specified USD path is actually a USD repo

    Args:
        usd_dir (str):

    Raises:
        RuntimeError: If usd_dir is not a path to valid USD repo
    """
    if not os.path.exists(usd_dir) or not os.path.isdir(usd_dir):
        raise RuntimeError("{} is not a valid directory".format(usd_dir))

    pxr_h = os.path.join(usd_dir, "pxr", "pxr.h.in")
    if not os.path.exists(pxr_h):
        raise RuntimeError("{} is not a valid USD repo".format(usd_dir))

    global USD_DIR
    USD_DIR = usd_dir


def run():
    parser = argparse.ArgumentParser(
        prog="merge_plugins.py",
        description="Merge or copy the dwa_usd_plugins repo into "
                    "a standard USD repo",
    )
    parser.add_argument("usd_dir", help="Path to the USD repository")
    for plugin_name, plugin_info in AVAILABLE_PLUGINS.items():
        parser.add_argument(
            "--{}".format(plugin_name),
            action="store_true",
            help=plugin_info["about"],
        )

    opts = vars(parser.parse_args())

    validate_usd_dir(opts["usd_dir"])

    for plugin_name in AVAILABLE_PLUGINS:
        if opts[plugin_name]:
            merge_plugin(plugin_name)


if __name__ == "__main__":
    run()

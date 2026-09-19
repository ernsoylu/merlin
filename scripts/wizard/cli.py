#!/usr/bin/env python3
"""Merlin wizard CLI.

Every subcommand from PROJECT_DEFINITION.md 5.1 is registered here from the
start, because install.sh, CI and the CMake pre-build target all spell these
names and the spelling is part of the contract. Only `check-env` does anything
yet; the rest report that they are unimplemented.

Exit codes (contract, 5.1):
    0  ok
    1  validation error
    2  environment error / not implemented
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard.core import env  # noqa: E402

OK, VALIDATION_ERROR, ENV_ERROR = 0, 1, 2

# name -> (help, phase that implements it)
COMMANDS = {
    "new": ("interactive S1-S9, creates project.json", 5),
    "configure": ("load and edit an existing project.json", 5),
    "add": ("add a device instance, module or swc instance", 5),
    "remove": ("remove a device instance, module or swc instance", 5),
    "generate": ("generate the ESP-IDF project", 5),
    "validate": ("schema and semantic validation only", 5),
    "resolve": ("refresh project.lock after source or toolchain changes", 5),
    "allocate": ("resource and pin allocation only", 5),
    "rte": ("port wiring only", 5),
    "audit": ("verify lock source hashes against the working tree", 5),
    "print-schema": ("dump a schema", 3),
    "device-security": ("guarded one-way eFuse / flash-encryption flow", 8),
}


def build_parser():
    parser = argparse.ArgumentParser(prog="merlin", description=__doc__.splitlines()[0])
    subs = parser.add_subparsers(dest="command", required=True, metavar="COMMAND")

    check = subs.add_parser("check-env", help="verify the toolchain against toolchain.env")
    check.set_defaults(handler=lambda args: env.check_env())

    for name, (help_text, phase) in COMMANDS.items():
        sub = subs.add_parser(name, help=help_text)
        sub.add_argument("args", nargs="*", help=argparse.SUPPRESS)
        sub.set_defaults(handler=_unimplemented(name, phase))

    return parser


def _unimplemented(name, phase):
    def handler(args):
        print(
            f"merlin {name}: not implemented (phase {phase}; see NEXT_STEPS.md)",
            file=sys.stderr,
        )
        return ENV_ERROR

    return handler


def main(argv=None):
    args = build_parser().parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())

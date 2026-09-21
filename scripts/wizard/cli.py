#!/usr/bin/env python3
"""Merlin wizard CLI.

Exit codes (contract, 5.1):
    0  ok
    1  validation error
    2  environment error
"""

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from scripts.wizard.core import env  # noqa: E402
from scripts.wizard.core.allocate import validate_target_claims  # noqa: E402
from scripts.wizard.core.generate import generate_project, write_platformio  # noqa: E402
from scripts.wizard.core.lock import audit_lock, write_lock  # noqa: E402
from scripts.wizard.core.model import load_project, resource_claims  # noqa: E402
from scripts.wizard.core.rte import resolve_connections  # noqa: E402
from scripts.wizard.core.validate import validate_project, validation_report  # noqa: E402

OK, VALIDATION_ERROR, ENV_ERROR = 0, 1, 2

PROJECT_FILE = "project.json"

# name -> (help, phase that implements it)
COMMANDS = {
    "new": ("create a guided current-scope project.json composition", 5),
    "configure": ("load and edit an existing project.json", 5),
    "add": ("add a device instance, module or swc instance", 5),
    "remove": ("remove a device instance, module or swc instance", 5),
    "generate": ("generate the ESP-IDF project", 5),
    "platformio": ("write an unpinned best-effort PlatformIO adapter", 7),
    "validate": ("schema and semantic validation only", 5),
    "resolve": ("refresh project.lock after source or toolchain changes", 5),
    "allocate": ("resource and pin allocation only", 5),
    "rte": ("port wiring only", 5),
    "audit": ("verify lock source hashes against the working tree", 5),
    "print-schema": ("dump a schema", 3),
    "device-security": ("guarded one-way eFuse / flash-encryption flow", 8),
}


_EXPLICIT_COMMANDS = {"print-schema", "validate", "generate", "platformio", "new"}


def _default_arguments(sub):
    sub.add_argument("args", nargs="*", help=argparse.SUPPRESS)


def _configure_arguments(sub):
    sub.add_argument("path", nargs="?", default=PROJECT_FILE)
    sub.add_argument("--set", dest="sets", action="append", default=[], metavar="KEY=VALUE")


def _add_arguments(sub):
    sub.add_argument("path", nargs="?", default=PROJECT_FILE)
    sub.add_argument("kind", choices=("device", "swc", "module"))
    sub.add_argument("name")
    sub.add_argument("type", nargs="?")
    sub.add_argument("properties", nargs="*")


def _remove_arguments(sub):
    sub.add_argument("path", nargs="?", default=PROJECT_FILE)
    sub.add_argument("kind", choices=("device", "swc", "module"))
    sub.add_argument("name")


_SUBCOMMAND_ARGUMENTS = {
    "configure": _configure_arguments,
    "add": _add_arguments,
    "remove": _remove_arguments,
}


def build_parser():
    parser = argparse.ArgumentParser(prog="merlin", description=__doc__.splitlines()[0])
    subs = parser.add_subparsers(dest="command", required=True, metavar="COMMAND")

    check = subs.add_parser("check-env", help="verify the toolchain against toolchain.env")
    check.add_argument("--target", choices=("esp32", "esp8266", "all"), default="esp32")
    check.set_defaults(handler=lambda args: env.check_env(target=args.target))

    schema = subs.add_parser("print-schema", help="print one frozen JSON schema")
    schema.add_argument("name", nargs="?", default="project", choices=sorted({"project", "lock", "interface", "driver", "swc", "soc", "module", "board", "overlay"}))
    schema.set_defaults(handler=_print_schema)

    validate = subs.add_parser("validate", help="validate a project")
    validate.add_argument("path", nargs="?", default=PROJECT_FILE)
    validate.set_defaults(handler=_validate)

    generate = subs.add_parser("generate", help="generate a current-scope reference project")
    generate.add_argument("path", nargs="?", default=PROJECT_FILE)
    generate.add_argument("--output", default=None)
    generate.add_argument("--frozen", action="store_true")
    generate.add_argument("--non-interactive", action="store_true", help="disable prompts (default for current-scope generation)")
    generate.add_argument("--ack", action="append", default=[], metavar="VAL-ID:OBJECT")
    generate.set_defaults(handler=_generate)

    platformio = subs.add_parser("platformio", help="write a best-effort PlatformIO adapter")
    platformio.add_argument("path", nargs="?", default=PROJECT_FILE)
    platformio.add_argument("--output", default=None)
    platformio.set_defaults(handler=_platformio)

    new = subs.add_parser("new", help="create a guided current-scope project composition")
    new.add_argument("--reference", choices=["climate-demo", "hw364a-oled-demo"], default="climate-demo")
    new.add_argument("--output", default=PROJECT_FILE)
    new.add_argument("--non-interactive", action="store_true", help="create the selected current-scope template without prompts")
    new.set_defaults(handler=_new)

    for name, (help_text, phase) in COMMANDS.items():
        if name in _EXPLICIT_COMMANDS:
            continue
        sub = subs.add_parser(name, help=help_text)
        _SUBCOMMAND_ARGUMENTS.get(name, _default_arguments)(sub)
        sub.set_defaults(handler=_HANDLERS.get(name, _unimplemented(name, phase)))

    return parser


def _print_schema(args):
    path = Path(__file__).resolve().parents[1] / "schemas" / f"{args.name}.json"
    print(path.read_text(encoding="utf-8"), end="")
    return OK


def _validate(args):
    report = validation_report(args.path)
    if report["errors"]:
        for error in report["errors"]:
            print(error, file=sys.stderr)
        return VALIDATION_ERROR
    for warning in report["warnings"]:
        print(warning, file=sys.stderr)
    print(f"valid: {Path(args.path)}")
    return OK


def _generate(args):
    project = Path(args.path).resolve()
    output = Path(args.output).resolve() if args.output else project.parent / "code"
    try:
        generate_project(project, output, frozen=args.frozen, acknowledgements=args.ack)
    except (OSError, ValueError) as error:
        print(f"generate: {error}", file=sys.stderr)
        return VALIDATION_ERROR
    print(f"generated: {output}")
    return OK


def _new(args):
    reference = args.reference
    name = None
    if not args.non_interactive and sys.stdin.isatty() and sys.stdout.isatty():
        try:
            import questionary

            reference = questionary.select(
                "S2 target/reference",
                choices=["climate-demo", "hw364a-oled-demo"],
                default=reference,
            ).ask()
            if reference is None:
                return VALIDATION_ERROR
            default_name = reference
            name = questionary.text("S1 project name", default=default_name).ask()
            if not name:
                return VALIDATION_ERROR
            if not questionary.confirm("S9 create this qualified composition?", default=True).ask():
                return VALIDATION_ERROR
        except (EOFError, KeyboardInterrupt):
            print("new: cancelled", file=sys.stderr)
            return VALIDATION_ERROR

    fixture = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / (
        "00-climate-demo" if reference == "climate-demo" else "01-hw364a-oled-demo"
    ) / PROJECT_FILE
    output = Path(args.output).resolve()
    if output.exists():
        print(f"new: refusing to overwrite {output}", file=sys.stderr)
        return VALIDATION_ERROR
    data = json.loads(fixture.read_text(encoding="utf-8"))
    if name:
        data["project"]["name"] = name
        output.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    else:
        output.write_bytes(fixture.read_bytes())
    if name:
        print("S1 metadata: accepted")
        print(f"S2 target: {data['target']['ecu']} / {data['target']['board']}")
        print("S3-S8: derived from the qualified reference composition")
        print("S9 review: validation and lock preview run at generate time")
    print(f"created: {output}")
    return OK


def _platformio(args):
    project = Path(args.path).resolve()
    output = Path(args.output).resolve() if args.output else project.parent / "code"
    try:
        path = write_platformio(project, output)
    except (OSError, ValueError) as error:
        print(f"platformio: {error}", file=sys.stderr)
        return VALIDATION_ERROR
    print(f"platformio: {path}")
    return OK


def _audit(args):
    path = Path(args.args[0] if args.args else "project.lock")
    errors = audit_lock(path)
    for error in errors:
        print(error, file=sys.stderr)
    return VALIDATION_ERROR if errors else OK


def _resolve(args):
    project_path = Path(args.args[0] if args.args else PROJECT_FILE)
    model = load_project(project_path)
    write_lock(model, project_path.resolve().parent / "project.lock")
    print("resolved: project.lock")
    return OK


def _allocate(args):
    model = load_project(args.args[0] if args.args else PROJECT_FILE)
    claims = resource_claims(model)
    conflicts = validate_target_claims(model)
    print(json.dumps({"claims": claims, "conflicts": conflicts}, indent=2, sort_keys=True))
    return VALIDATION_ERROR if conflicts else OK


def _rte(args):
    model = load_project(args.args[0] if args.args else PROJECT_FILE)
    try:
        print(json.dumps(resolve_connections(model["project"], model["root"]), indent=2, sort_keys=True))
    except ValueError as error:
        print(f"rte: {error}", file=sys.stderr)
        return VALIDATION_ERROR
    return OK


def _parse_value(value):
    try:
        return json.loads(value)
    except json.JSONDecodeError:
        return value


def _set_path(data, dotted, value):
    parts = dotted.split(".")
    if not all(parts):
        raise ValueError(f"invalid setting path: {dotted}")
    current = data
    for part in parts[:-1]:
        if not isinstance(current, dict):
            raise ValueError(f"setting path is not an object: {dotted}")
        current = current.setdefault(part, {})
    if not isinstance(current, dict):
        raise ValueError(f"setting path is not an object: {dotted}")
    current[parts[-1]] = value


def _edit_project(path, mutate):
    # The path is the manifest the operator named on their own command line, so
    # there is no privilege boundary for a traversal to cross; reject anything
    # that is not an existing JSON file before touching it.
    path = Path(path).resolve()
    if path.suffix != ".json" or not path.is_file():
        raise ValueError(f"not an existing JSON manifest: {path}")
    original = path.read_bytes()  # NOSONAR (pythonsecurity:S2083)
    data = json.loads(original)
    mutate(data)
    path.write_text(  # NOSONAR (pythonsecurity:S2083)
        json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    errors = validate_project(path)
    if errors:
        path.write_bytes(original)
        raise ValueError("\n".join(errors))
    return path


def _configure(args):
    if not args.sets:
        print("configure: use --set KEY=VALUE", file=sys.stderr)
        return VALIDATION_ERROR
    try:
        def mutate(data):
            for assignment in args.sets:
                key, separator, value = assignment.partition("=")
                if not separator:
                    raise ValueError(f"configure: expected KEY=VALUE, got {assignment}")
                _set_path(data, key, _parse_value(value))
        _edit_project(args.path, mutate)
    except (OSError, ValueError) as error:
        print(f"configure: {error}", file=sys.stderr)
        return VALIDATION_ERROR
    print(f"configured: {Path(args.path).resolve()}")
    return OK


def _properties(values):
    result = {}
    for assignment in values:
        key, separator, value = assignment.partition("=")
        if not separator or not key:
            raise ValueError(f"expected KEY=VALUE, got {assignment}")
        result[key] = _parse_value(value)
    return result


def _add(args):
    try:
        properties = _properties(args.properties)
        if args.kind == "module":
            if args.type is not None:
                raise ValueError("module does not accept a type")
            def mutate(data):
                modules = data.setdefault("modules", [])
                if args.name in modules:
                    raise ValueError(f"module already present: {args.name}")
                modules.append(args.name)
                modules.sort()
        else:
            if not args.type:
                raise ValueError(f"{args.kind} requires TYPE")
            key = "devices" if args.kind == "device" else "swcs"
            def mutate(data):
                items = data.setdefault("instances", {}).setdefault(key, [])
                if any(item.get("instance") == args.name for item in items):
                    raise ValueError(f"instance already present: {args.name}")
                item = {"instance": args.name, "type": args.type, **properties}
                items.append(item)
                items.sort(key=lambda entry: entry["instance"])
        _edit_project(args.path, mutate)
    except (OSError, ValueError) as error:
        print(f"add: {error}", file=sys.stderr)
        return VALIDATION_ERROR
    print(f"added: {args.kind}/{args.name}")
    return OK


def _remove(args):
    try:
        def mutate(data):
            if args.kind == "module":
                modules = data.setdefault("modules", [])
                if args.name not in modules:
                    raise ValueError(f"module not present: {args.name}")
                modules.remove(args.name)
                return
            key = "devices" if args.kind == "device" else "swcs"
            items = data.setdefault("instances", {}).setdefault(key, [])
            before = len(items)
            items[:] = [item for item in items if item.get("instance") != args.name]
            if len(items) == before:
                raise ValueError(f"instance not present: {args.name}")
            prefix = args.name + "."
            data["connections"] = [
                item for item in data.get("connections", [])
                if not item.get("from", "").startswith(prefix)
                and not item.get("to", "").startswith(prefix)
            ]
        _edit_project(args.path, mutate)
    except (OSError, ValueError) as error:
        print(f"remove: {error}", file=sys.stderr)
        return VALIDATION_ERROR
    print(f"removed: {args.kind}/{args.name}")
    return OK


def _unimplemented(name, phase):
    def handler(args):
        print(
            f"merlin {name}: not implemented (phase {phase}; see NEXT_STEPS.md)",
            file=sys.stderr,
        )
        return ENV_ERROR

    return handler


_HANDLERS = {
    "audit": _audit,
    "resolve": _resolve,
    "allocate": _allocate,
    "rte": _rte,
    "configure": _configure,
    "add": _add,
    "remove": _remove,
}


def main(argv=None):
    args = build_parser().parse_args(argv)
    return args.handler(args)


if __name__ == "__main__":
    sys.exit(main())

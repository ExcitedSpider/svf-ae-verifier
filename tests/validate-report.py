"""Validate verifier reports against schema/report.schema.json.

usage: validate-report.py SCHEMA REPORT...
       validate-report.py SCHEMA --markdown FILE   (validates each ```json block)

Exits 0 when every document is valid, 1 otherwise.
"""
import json
import re
import sys

import jsonschema


def documents(args):
    if args[0] == "--markdown":
        with open(args[1], encoding="utf-8") as f:
            blocks = re.findall(r"```json\n(.*?)```", f.read(), re.S)
        if not blocks:
            sys.exit(f"{args[1]}: no ```json block")
        for i, block in enumerate(blocks):
            yield f"{args[1]} block {i + 1}", json.loads(block)
        return
    for path in args:
        with open(path, encoding="utf-8") as f:
            yield path, json.load(f)


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        schema = json.load(f)
    validator_class = jsonschema.validators.validator_for(schema)
    validator_class.check_schema(schema)
    validator = validator_class(schema)
    failed = False
    for name, document in documents(sys.argv[2:]):
        for error in sorted(validator.iter_errors(document), key=lambda e: list(e.path)):
            failed = True
            path = "/".join(str(p) for p in error.absolute_path)
            print(f"{name}: /{path}: {error.message}", file=sys.stderr)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()

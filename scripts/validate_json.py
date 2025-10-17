import json
import os
import sys
import traceback

json_files = [
    "canada.json",
    "citm_catalog.json",
    "twitter.json"
]

def main() -> int:
    tests_data_dir = os.path.dirname(os.path.dirname(__file__)) + "/tests_data/json"

    for json_file in json_files:
        in_json_path = f"{tests_data_dir}/{json_file}"
        out_json_path = f"{tests_data_dir}/out_{json_file}"

        try:
            with open(in_json_path, "r", encoding="utf-8") as in_file:
                in_json = json.load(in_file)

            with open(out_json_path, "r", encoding="utf-8") as out_file:
                out_json = json.load(out_file)

            if not (sorted(in_json) == sorted(out_json)):
                print(f"Error while comparing {json_file}: comparison is false")
                return 1
        except:
            print(f"Error while processing {json_file}:\n{traceback.format_exc()}")
            return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
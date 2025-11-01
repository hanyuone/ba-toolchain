import re

FUNCTION_NAME = "main"
UNNUMBERED = 481
SUBTRACT_BY = 38

if __name__ == "__main__":
    with open("../build/vsa_modified.ll") as f:
        lines = f.readlines()
        in_range = False

        for line in lines:
            if not in_range and not line.startswith(f"define ptr @LIFTED.{FUNCTION_NAME}"):
                continue

            in_range = True
            line = line[:-1]

            if line == "}":
                break

            for match in reversed(list(re.finditer(r"%\d+", line))):
                num = int(match.group(0)[1:])
                span = match.span()
                
                if num >= UNNUMBERED:
                    updated_num = num - SUBTRACT_BY
                    line = line[:span[0]] + "%" + str(updated_num) + line[span[1]:]

            if re.match(r"^\d+:", line):
                colon_index = line.find(":")
                num = int(line[:colon_index])

                if num >= UNNUMBERED:
                    updated_num = num - SUBTRACT_BY
                    line = str(updated_num) + line[colon_index:]

            print(line)

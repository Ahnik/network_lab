import re
import sys

SCHEMES = [
    "CHECKSUM",
    "CRC8",
    "CRC10",
    "CRC16",
    "CRC32"
]

def parse_sender_log(filename):
    sender = {scheme: {} for scheme in SCHEMES}

    index = 0
    current_frame = None
    previous_frame = None

    with open(filename, "r", encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            match = re.match(r"---\s*FRAME\s*#(\d+)\s*---", line)

            if match:
                current_frame = int(match.group(1))

                if (previous_frame is not None and current_frame < previous_frame):
                    index += 1
                previous_frame = current_frame
                continue

            if current_frame is None:
                continue

            if "NONE" in line:
                error_type = "NONE"
            elif "BURST" in line:
                error_type = "BURST"
            elif "SINGLE" in line:
                error_type = "SINGLE"
            elif "ISOLATED" in line:
                error_type = "ISOLATED"
            elif "ODD" in line:
                error_type = "ODD"

            scheme = SCHEMES[index]
            sender[scheme][current_frame] = error_type
    return sender

def parse_receiver_log(filename):
    receiver = {scheme: {} for scheme in SCHEMES}

    index = 0
    current_frame = None
    previous_frame = None

    with open(filename, "r", encoding="utf-8") as file:
        for line in file:
            line = line.strip()
            match = re.match(r"---\s*FRAME\s*#(\d+)\s*---", line)

            if match:
                current_frame = int(match.group(1))

                if (previous_frame is not None and current_frame < previous_frame):
                    index += 1
                    if index >= len(SCHEMES):
                        raise ValueError(
                            "More scheme blocks found than "
                            "configured in SCHEMES."
                        )

                previous_frame = current_frame
                continue

            if current_frame is None:
                continue

            match = re.match(
                r"(CHECKSUM|CRC8|CRC10|CRC16|CRC32)"
                r"\s*:\s*"
                r"(VALID|CORRUPTED)",
                line,
                re.IGNORECASE
            )

            if match:
                result = match.group(2).upper()
                scheme = SCHEMES[index]
                receiver[scheme][current_frame] = result
    return receiver

def analyze(sender, receiver):
    statistics = {}
    for scheme in SCHEMES:
        statistics[scheme] = {}
        common_frames = (set(sender[scheme].keys()) & set(receiver[scheme].keys()))

        for frame_number in common_frames:
            error_type = sender[scheme][frame_number]
            result = receiver[scheme][frame_number]

            if error_type == "None":
                continue
            if error_type not in statistics[scheme]:
                statistics[scheme][error_type] = {
                    "total": 0,
                    "detected": 0,
                    "missed": 0
                }

            data = statistics[scheme][error_type]
            data["total"] += 1
            if result == "CORRUPTED":
                data["detected"] += 1
            elif result == "VALID":
                data["missed"] += 1
    return statistics

def write_report(filename, statistics):
    error_types = set()
    for scheme in SCHEMES:
        error_types.update(statistics[scheme].keys())
    error_types = sorted(error_types)

    with open(filename, "w", encoding="utf-8") as f:
        f.write("ERROR DETECTION STATISTICS\n")
        f.write("==========================\n\n")
        f.write(
            f"Detection schemes : {', '.join(SCHEMES)}\n\n"
        )

        f.write("Detection Rate\n")
        f.write("==============\n\n")

        header = f"{'Error Type':<20}"

        for scheme in SCHEMES:
            header += f"{scheme:>12}"

        f.write(header + "\n")
        f.write("-" * len(header) + "\n")

        for error_type in error_types:

            row = f"{error_type:<20}"

            for scheme in SCHEMES:

                if error_type not in statistics[scheme]:
                    row += f"{'N/A':>12}"
                    continue

                data = statistics[scheme][error_type]

                total = data["total"]
                detected = data["detected"]

                if total == 0:
                    rate = 0.0
                else:
                    rate = detected / total * 100

                row += f"{rate:>11.2f}%"

            f.write(row + "\n")

        f.write("\n\n")
        f.write("Detailed Statistics\n")
        f.write("===================\n\n")

        for scheme in SCHEMES:

            f.write(f"SCHEME: {scheme}\n")
            f.write("-" * 70 + "\n")

            header = (
                f"{'Error Type':<20}"
                f"{'Detected':>12}"
                f"{'Missed':>10}"
                f"{'Total':>10}"
                f"{'Rate':>12}"
            )

            f.write(header + "\n")
            f.write("-" * len(header) + "\n")

            for error_type in error_types:

                if error_type not in statistics[scheme]:
                    continue

                data = statistics[scheme][error_type]

                total = data["total"]
                detected = data["detected"]
                missed = data["missed"]

                rate = (
                    detected / total * 100
                    if total > 0
                    else 0.0
                )

                f.write(
                    f"{error_type:<20}"
                    f"{detected:>12}"
                    f"{missed:>10}"
                    f"{total:>10}"
                    f"{rate:>11.2f}%\n"
                )

            f.write("\n")

def main():
    if len(sys.argv) != 4:
        print(
            f"Usage: {sys.argv[0]} "
            f"<sender_log> <receiver_log> <output_file>"
        )
        sys.exit(1)

    sender_file = sys.argv[1]
    receiver_file = sys.argv[2]
    output_file = sys.argv[3]

    print("Parsing sender log...")
    sender = parse_sender_log(sender_file)

    print("Parsing receiver log...")
    receiver = parse_receiver_log(receiver_file)

    print("Analyzing logs...")
    statistics = analyze(sender, receiver)

    print("Writing report...")
    write_report(output_file, statistics)

    print(f"\nReport written to: {output_file}")


if __name__ == "__main__":
    main()
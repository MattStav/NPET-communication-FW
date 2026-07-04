from pathlib import Path
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="Process the MANUAL.md file and generate a header file with its content.")
parser.add_argument("-s", "--source", required=True, help="Path to the source MANUAL.md", type=str)
parser.add_argument("-d", "--destination",
                    help="Path to the destination for the header file (if dir, the header will be created as `manual_data.h`)",
                    default=str(Path.cwd()),
                    type=str
                    )
args = parser.parse_args()
# Validate paths
source_path = Path(args.source).resolve()
if not source_path.is_file():
    raise FileNotFoundError(f"Source file not found: {source_path}")
if not source_path.name == "MANUAL.md":
    raise FileNotFoundError(f"Invalid source file name: {source_path}")
destination_path = Path(args.destination).resolve()
if not destination_path.suffix:
    if not destination_path.exists():
        raise FileNotFoundError(f"Destination directory not found: {destination_path}")
    destination_path = destination_path / "manual_data.h"
# Copy source content to destination
content: str = source_path.read_text()
escaped = content.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n\"\n\"")
assert destination_path.suffix == ".h", f"Destination must be a header file: {destination_path}"
destination_path.write_text(f'#pragma once\ninline const char* manual_text = "{escaped}";')

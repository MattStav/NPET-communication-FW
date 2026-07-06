from pathlib import Path
import argparse

# Parse command-line arguments
parser = argparse.ArgumentParser(description="Process a text file and generate a header file with its content.")
parser.add_argument("-s", "--source", required=True, help="Path to the source file", type=str)
parser.add_argument("-d", "--destination",
                    help="Path to the destination for the header file",
                    default=str(Path.cwd()),
                    type=str,
                    )
args = parser.parse_args()
# Validate paths
source_path = Path(args.source).resolve()
if not source_path.is_file():
    raise FileNotFoundError(f"Source file not found: {source_path}")
if source_path.suffix not in [".md", ".txt", ""]:
    raise FileNotFoundError(f"Invalid source file suffix: {source_path}")
destination_path = Path(args.destination).resolve()
if not destination_path.parent.is_dir():
    raise FileNotFoundError(f"Destination directory not found: {destination_path}")
if not destination_path.suffix == ".h":
    raise ValueError(f"Destination must be a header file: {destination_path}")
# Copy source content to destination
content: str = source_path.read_text()
escaped = content.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n\"\n\"")
destination_path.write_text(f'#pragma once\ninline const char* {source_path.stem.lower()}_text = "{escaped}";')

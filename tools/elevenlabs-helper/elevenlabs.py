import os
import argparse
import json
from pathlib import Path

import requests


VOICE_ID = "JBFqnCBsd6RMkjVDRZzb"
MODEL_ID = "eleven_multilingual_v2"
OUTPUT_FORMAT = "mp3_44100_128"


def tts_request(api_key: str, text: str) -> bytes:
    url = f"https://api.elevenlabs.io/v1/text-to-speech/{VOICE_ID}?output_format={OUTPUT_FORMAT}"
    response = requests.post(
        url,
        headers={"xi-api-key": api_key},
        json={"text": text, "model_id": MODEL_ID},
        timeout=30,
    )
    if not response.ok:
        raise RuntimeError(f"API error {response.status_code}: {response.text}")
    return response.content


def write_mp3(output_path: Path, audio_bytes: bytes) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(audio_bytes)


def collect_audio_entries(payload: object) -> list[tuple[str, str]]:
    entries: list[tuple[str, str]] = []

    def walk(node: object) -> None:
        if isinstance(node, dict):
            text = node.get("text")
            file_name = node.get("file")
            if isinstance(text, str) and isinstance(file_name, str):
                entries.append((text, file_name))
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(payload)
    return entries


def validate_json_payload(payload: object) -> tuple[list[str], list[tuple[str, str]]]:
    errors: list[str] = []
    if not isinstance(payload, dict):
        return ["Top level JSON must be an object."], []

    entries = collect_audio_entries(payload)
    if not entries:
        errors.append("No audio entries found. Expected objects with 'text' and 'file'.")
        return errors, []

    seen_files: set[str] = set()
    for i, (text, file_name) in enumerate(entries):
        if not isinstance(text, str) or not text.strip():
            errors.append(f"Entry #{i}.text must be a non-empty string.")

        if not isinstance(file_name, str) or not file_name.strip():
            errors.append(f"Entry #{i}.file must be a non-empty string.")
            continue
        if Path(file_name).name != file_name:
            errors.append(f"Entry #{i}.file must be a file name only, not a path.")
        elif not file_name.lower().endswith(".mp3"):
            errors.append(f"Entry #{i}.file must end with .mp3.")
        if file_name in seen_files:
            errors.append(f"Duplicate file name detected: {file_name}")
        seen_files.add(file_name)

    return errors, entries


def download_from_json(api_key: str, json_path: Path) -> None:
    payload = json.loads(json_path.read_text(encoding="utf-8"))
    errors, entries = validate_json_payload(payload)
    if errors:
        raise ValueError("Invalid JSON:\n- " + "\n- ".join(errors))

    output_dir = json_path.parent
    downloaded = 0
    skipped = 0
    for text, file_name in entries:
        target_path = output_dir / file_name
        if target_path.exists():
            skipped += 1
            print(f"Skipped existing: {target_path}")
            continue

        audio_bytes = tts_request(api_key, text)
        write_mp3(target_path, audio_bytes)
        downloaded += 1
        print(f"Saved: {target_path}")

    print(f"Done. Downloaded {downloaded} files, skipped {skipped} existing files.")


def download_from_text(api_key: str, text: str, output_file: str) -> None:
    audio_bytes = tts_request(api_key, text)
    target = Path(output_file)
    write_mp3(target, audio_bytes)
    print(f"Audio saved to {target}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate ElevenLabs MP3 from text or from a JSON batch file."
    )
    parser.add_argument(
        "input",
        help="Text to synthesize OR path to a .json file.",
    )
    parser.add_argument(
        "-o",
        "--output",
        default="output.mp3",
        help="Output filename for single-text mode (default: output.mp3).",
    )
    return parser.parse_args()


def main() -> None:
    api_key = os.getenv("ELEVENLABS_API_KEY")
    if not api_key:
        raise ValueError("Set ELEVENLABS_API_KEY environment variable.")

    args = parse_args()
    input_value = args.input

    possible_json = Path(input_value)
    if possible_json.suffix.lower() == ".json" and possible_json.exists():
        download_from_json(api_key, possible_json)
    else:
        download_from_text(api_key, input_value, args.output)


if __name__ == "__main__":
    main()
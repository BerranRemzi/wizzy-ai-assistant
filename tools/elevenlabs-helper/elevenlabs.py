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


def validate_json_payload(payload: object) -> list[str]:
    errors: list[str] = []
    if not isinstance(payload, dict):
        return ["Top level JSON must be an object mapping category -> list of items."]

    for category, items in payload.items():
        if not isinstance(items, list):
            errors.append(f"Category '{category}' must be a list.")
            continue

        for i, item in enumerate(items):
            if not isinstance(item, dict):
                errors.append(f"{category}[{i}] must be an object.")
                continue

            text = item.get("text")
            file_name = item.get("file")

            if not isinstance(text, str) or not text.strip():
                errors.append(f"{category}[{i}].text must be a non-empty string.")

            if not isinstance(file_name, str) or not file_name.strip():
                errors.append(f"{category}[{i}].file must be a non-empty string.")
            elif Path(file_name).name != file_name:
                errors.append(f"{category}[{i}].file must be a file name only, not a path.")
            elif not file_name.lower().endswith(".mp3"):
                errors.append(f"{category}[{i}].file must end with .mp3.")

    return errors


def download_from_json(api_key: str, json_path: Path) -> None:
    payload = json.loads(json_path.read_text(encoding="utf-8"))
    errors = validate_json_payload(payload)
    if errors:
        raise ValueError("Invalid JSON:\n- " + "\n- ".join(errors))

    output_dir = json_path.parent
    total = 0
    for _, items in payload.items():
        for item in items:
            text = item["text"]
            file_name = item["file"]
            target_path = output_dir / file_name

            audio_bytes = tts_request(api_key, text)
            write_mp3(target_path, audio_bytes)
            total += 1
            print(f"Saved: {target_path}")

    print(f"Done. Downloaded {total} files.")


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
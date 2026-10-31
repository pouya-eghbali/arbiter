#!/usr/bin/env python3
"""Render isolated mobile UI fixtures across phone sizes; never uses a real profile."""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import re
import subprocess

SCREENS = [
    "start", "browsing", "search", "tabs", "workspaces", "more", "settings",
    *[f"settings:{category}" for category in ("appearance", "language", "search", "privacy", "sleep", "sync", "about")],
    "settings:appearance:bottom", "settings:privacy:bottom", "history", "history:empty",
    "downloads", "downloads:empty", "vault", "pair", "pairReview", "scan", "save", "accounts",
    *[f"onboarding:{step}" for step in range(4)], "workspaces:editor", "private", "suggestions",
    "search:keyboard", "save:keyboard", "workspaces:editor:keyboard",
]
ERROR = re.compile(r"qrc:|ReferenceError|TypeError|binding loop|Cannot assign", re.IGNORECASE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, required=True, help="Desktop app built with ARBITER_MOBILE_DESIGN_PREVIEW=ON")
    parser.add_argument("--output", type=Path, default=Path("output/mobile-ui-audit"))
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = [(screen, 390, 844, "dark") for screen in SCREENS]
    cases += [(screen, 320, 568, "light") for screen in ("start", "tabs", "more", "search", "settings:appearance", "settings:privacy", "onboarding:1", "workspaces:editor", "save", "history")]
    cases += [(screen, 844, 390, "dark") for screen in ("start", "tabs", "more", "search", "settings:privacy", "onboarding:1")]

    def render(case):
        screen, width, height, theme = case
        name = f"{screen.replace(':', '-')}-{width}x{height}-{theme}"
        env = os.environ | {"QT_QPA_PLATFORM": "offscreen", "QT_SCALE_FACTOR": "2", "ARBITER_PREVIEW_WIDTH": str(width), "ARBITER_PREVIEW_HEIGHT": str(height), "ARBITER_PREVIEW_THEME": theme}
        output = args.output / f"{name}.png"
        result = subprocess.run([str(args.app.resolve()), f"--design-screen={screen}", f"--design-output={output.resolve()}"], env=env, capture_output=True, text=True, timeout=45)
        log = result.stdout + result.stderr
        (args.output / f"{name}.log").write_text(log)
        problems = [line for line in log.splitlines() if ERROR.search(line)]
        return {"screen": screen, "size": [width, height], "theme": theme, "passed": result.returncode == 0 and output.exists() and not problems, "problems": problems}

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        results = list(pool.map(render, cases))
    (args.output / "results.json").write_text(json.dumps(results, indent=2))
    failures = [result for result in results if not result["passed"]]
    for result in failures:
        print(result)
    print(f"{len(results) - len(failures)}/{len(results)} screen renders passed. Inspect images in {args.output}.")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())

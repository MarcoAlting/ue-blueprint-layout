"""Install the ue-blueprint-layout skill and the BlueprintLayoutToolset Unreal plugin.

Usage:
    python install.py skill                        copy the skill to ~/.claude/skills
    python install.py plugin <project folder>      copy the plugin to <project>/Plugins
    python install.py plugin <project folder> --gitignore
                                                   also add the plugin folder to the project's .gitignore

Running a command again replaces the installed copy with the version in this repository.
"""
import argparse
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent
SKILL_NAME = "ue-blueprint-layout"
PLUGIN_NAME = "BlueprintLayoutToolset"
BUILD_FOLDERS = ("Binaries", "Intermediate")


def replace_tree(source: Path, target: Path, ignore=None) -> None:
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target, ignore=ignore)


def install_skill(_args) -> None:
    target = Path.home() / ".claude" / "skills" / SKILL_NAME
    replace_tree(REPO / "skill" / SKILL_NAME, target)
    print(f"Skill installed: {target}")


def install_plugin(args) -> None:
    project = Path(args.project).resolve()
    if not any(project.glob("*.uproject")):
        sys.exit(f"No .uproject file in {project}")

    target = project / "Plugins" / PLUGIN_NAME
    target.parent.mkdir(exist_ok=True)
    # Keep the project's own build output out of the copy, and remove a stale one.
    replace_tree(REPO / "unreal" / PLUGIN_NAME, target, ignore=shutil.ignore_patterns(*BUILD_FOLDERS))
    print(f"Plugin installed: {target}")

    if args.gitignore:
        gitignore = project / ".gitignore"
        entry = f"/Plugins/{PLUGIN_NAME}/"
        lines = gitignore.read_text(encoding="utf-8").splitlines() if gitignore.exists() else []
        if entry not in lines:
            with gitignore.open("a", encoding="utf-8") as f:
                if lines and lines[-1].strip():
                    f.write("\n")
                f.write(f"# Installed from the ue-blueprint-layout repository\n{entry}\n")
            print(f"Added {entry} to {gitignore}")

    print("Next: close every Unreal Editor that uses this engine, build the editor target, and restart.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("skill", help="copy the skill to ~/.claude/skills").set_defaults(run=install_skill)
    plugin = commands.add_parser("plugin", help="copy the Unreal plugin into a project")
    plugin.add_argument("project", help="folder that contains the .uproject file")
    plugin.add_argument("--gitignore", action="store_true", help="add the plugin folder to the project's .gitignore")
    plugin.set_defaults(run=install_plugin)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()

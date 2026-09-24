"""Build one Win32 XP project with a case-normalized Windows environment."""
import argparse
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('project')
parser.add_argument('output')
probe = parser.add_mutually_exclusive_group()
probe.add_argument('--native-probe', action='store_true')
probe.add_argument('--bmg-native-probe', action='store_true')
probe.add_argument('--bmg-baseline-probe', action='store_true')
probe.add_argument('--ceiling-native-probe', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
msbuild = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe')
output = (root / args.output).resolve()
command = [str(msbuild), str((root / args.project).resolve()),
           '/p:Configuration=Release', '/p:Platform=Win32',
           f'/p:OutDir={output}\\', f'/p:IntDir={output / "obj"}\\',
           '/verbosity:quiet', '/nologo', '/clp:ErrorsOnly', '/m:1', '/nr:false']
if args.native_probe:
    command.append('/p:BlizzardNativeProbe=true')
if args.bmg_native_probe:
    command.append('/p:BmgNativeProbe=true')
if args.ceiling_native_probe:
    command.append('/p:CeilingNativeProbe=true')
if args.bmg_baseline_probe:
    command.append('/p:BmgBaselineProbe=true')
# Some launch environments contain both PATH and Path. The MSBuild host-tool
# task rejects that duplicate even though Windows treats it as one variable.
raise SystemExit(subprocess.call(command, cwd=root, env={key.upper(): value for key, value in os.environ.items()}))

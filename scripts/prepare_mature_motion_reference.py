"""Prepare an isolated, pinned ALS source study. Never edits Fireline Content.

Downloads from the author's repository only when the pinned archive is absent.
Use --build for the editor host; --launcher creates a separate macOS entry.
"""
from pathlib import Path
import argparse
import hashlib
import json
import plistlib
import shutil
import subprocess
import urllib.request
import zipfile

COMMIT = 'b754d6f0f2bb03741d301f8fb88077ebfe561e17'
SHA256 = 'c8affb459caa9037f9a1f017701ce31ffaebccaf54dd56006cf7af235c589e11'
ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--engine', type=Path, default=Path('/Users/Shared/Epic Games/UE_5.8'))
    p.add_argument('--lyra', type=Path, help='Optional existing official Lyra project directory')
    p.add_argument('--fireline', action='store_true', help='Read-only content link for native-model comparison')
    p.add_argument('--build', action='store_true')
    p.add_argument('--launcher', action='store_true')
    a = p.parse_args()
    out = ROOT / 'unreal/Fireline/Saved/MatureMotionResearch'
    out.mkdir(parents=True, exist_ok=True)
    archive = out / 'ALS-Refactored.zip'
    if not archive.exists():
        temp = archive.with_suffix('.download')
        urllib.request.urlretrieve(f'https://codeload.github.com/Sixze/ALS-Refactored/zip/{COMMIT}', temp)
        assert hashlib.sha256(temp.read_bytes()).hexdigest() == SHA256, 'Download hash mismatch'
        temp.replace(archive)
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == SHA256, 'Archive hash mismatch'
    source = out / f'ALS-Refactored-{COMMIT}'
    if not source.exists():
        with zipfile.ZipFile(archive) as z:
            for entry in z.infolist():
                assert (out / entry.filename).resolve().is_relative_to(out.resolve())
            z.extractall(out)
    project = out / 'ReferenceProject'
    module = project / 'Source/MotionReference'
    module.mkdir(parents=True, exist_ok=True)
    (project / 'Plugins').mkdir(exist_ok=True)
    plugin = project / 'Plugins/ALS'
    if not plugin.exists():
        plugin.symlink_to(source, target_is_directory=True)
    assert plugin.resolve() == source.resolve()
    config = {'FileVersion': 3, 'EngineAssociation': '5.8',
              'Modules': [{'Name': 'MotionReference', 'Type': 'Runtime', 'LoadingPhase': 'Default'}],
              'Plugins': [{'Name': n, 'Enabled': True} for n in
                          ['ALS', 'PythonScriptPlugin', 'EditorScriptingUtilities']]}
    uproject = project / 'ReferenceProject.uproject'
    uproject.write_text(json.dumps(config, indent=2))
    for path in (ROOT / 'scripts/reference_host').glob('*'):
        shutil.copyfile(path, module / path.name)
    (module / 'MotionReference.Build.cs').write_text('''using UnrealBuildTool;
public class MotionReference : ModuleRules {
 public MotionReference(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","ALS","GameplayTags","InputCore","UMG","EnhancedInput"});
 }
}
''')
    (project / 'Source/MotionReferenceEditor.Target.cs').write_text('''using UnrealBuildTool;
public class MotionReferenceEditorTarget : TargetRules {
 public MotionReferenceEditorTarget(TargetInfo Target) : base(Target) {
  Type=TargetType.Editor; DefaultBuildSettings=BuildSettingsVersion.V7;
  IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.Add("MotionReference");
 }
}
''')
    (project / 'Config').mkdir(exist_ok=True)
    shutil.copyfile(source / 'Config/Input.ini', project / 'Config/DefaultInput.ini')
    (project / 'Config/DefaultEngine.ini').write_text((source / 'Config/Engine.ini').read_text() + '''
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/ALS/ALSExtras/Levels/L_Als_Playground
GlobalDefaultGameMode=/ALS/ALSExtras/Core/B_Als_GameMode.B_Als_GameMode_C
''')
    if a.lyra:
        (project / 'Content').mkdir(exist_ok=True)
        for name in ['Characters', 'Weapons']:
            target = (a.lyra / 'Content' / name).resolve()
            assert target.is_dir(), str(target)
            link = project / 'Content' / name
            if not link.exists():
                link.symlink_to(target, target_is_directory=True)
            assert link.resolve() == target
    if a.fireline:
        (project / 'Content').mkdir(exist_ok=True)
        target = ROOT / 'unreal/Fireline/Content/Fireline'
        link = project / 'Content/Fireline'
        if not link.exists():
            link.symlink_to(target, target_is_directory=True)
        assert link.resolve() == target.resolve()
    if a.build:
        subprocess.run([str(a.engine / 'Engine/Build/BatchFiles/Mac/Build.sh'),
                        'MotionReferenceEditor', 'Mac', 'Development', str(uproject),
                        '-MaxParallelActions=2', '-WaitMutex'], check=True)
    if a.launcher:
        app = Path.home() / 'UnrealBuilds/Fireline/Launchers/FirelineALSReference.app'
        contents = app / 'Contents'
        for folder in ['MacOS', 'Resources']:
            (contents / folder).mkdir(parents=True, exist_ok=True)
        info = {'CFBundleExecutable': 'FirelineStudyLauncher',
                'CFBundleIdentifier': 'local.fireline.als-reference',
                'CFBundleName': '参考·ALS原版动作', 'CFBundlePackageType': 'APPL',
                'CFBundleVersion': '1', 'LSUIElement': True, 'NSHighResolutionCapable': True}
        settings = {'Candidate': f'Original ALS {COMMIT}; research only',
                    'Editor': str(a.engine / 'Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor'),
                    'Arguments': [str(uproject), '/ALS/ALSExtras/Levels/L_Als_Playground',
                                  '-game', '-ReferenceRiflePreview', '-windowed', '-ResX=1280', '-ResY=800',
                                  '-nosound', '-ExecCmds=t.MaxFPS 45,sg.ShadowQuality 0,sg.GlobalIlluminationQuality 0,sg.ReflectionQuality 0']}
        for name, obj in [('Info.plist', info), ('Resources/Study.plist', settings)]:
            with (contents / name).open('wb') as f:
                plistlib.dump(obj, f)
        launcher = (ROOT / 'scripts/LatestStudyLauncher.m').read_text()
        needle = '/Game/Fireline/Maps/FirelineRange.FirelineRange'
        assert needle in launcher
        temporary_source = out / 'ALSReferenceLauncher.m'
        temporary_source.write_text(launcher.replace(needle, '/ALS/ALSExtras/Levels/L_Als_Playground.L_Als_Playground'))
        subprocess.run(['xcrun', 'clang', '-fobjc-arc', '-framework', 'Cocoa', str(temporary_source),
                        '-o', str(contents / 'MacOS/FirelineStudyLauncher')], check=True)
        subprocess.run(['codesign', '--force', '--sign', '-', str(app)], check=True)
        desktop = Path.home() / 'Desktop/火力对决/参考·ALS原版动作.app'
        desktop.parent.mkdir(parents=True, exist_ok=True)
        if not desktop.exists():
            desktop.symlink_to(app, target_is_directory=True)
        assert desktop.resolve() == app.resolve()
        print(desktop)
    print(uproject)


if __name__ == '__main__':
    main()

"""Compare native raw replay and live-toggle restoration at identical times."""
import argparse, gzip, json, re
from pathlib import Path
import numpy as np
from study_surface_mount import BASE, load_pose, transform


def errors(actual, expected):
    assert actual.keys() == expected.keys()
    return [max(float(np.linalg.norm(actual[n][0] - expected[n][0])) for n in actual),
            max(float(np.degrees((actual[n][1] * expected[n][1].inv()).magnitude())) for n in actual),
            max(float(np.max(np.abs(actual[n][2] - expected[n][2]))) for n in actual)]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--raw-run', default='lyra-native-ads-raw-20261001')
    p.add_argument('--toggle-run', required=True)
    a = p.parse_args()
    source = BASE / 'ADSLayoutStudy'
    runs = BASE / 'ReferenceProject/Saved/LyraADSReview'
    meta = json.loads((source / 'lyra-postprocess.json').read_text())
    original = runs / meta['captured_run']
    raw = json.loads(gzip.decompress((source / 'lyra-raw-at-native-times.json.gz').read_bytes()))
    toggle = runs / a.toggle_run
    capture = json.loads((toggle / 'capture.json').read_text())
    assert capture['toggle_audit']
    events = [(int(frame), int(is_raw)) for frame, is_raw in re.findall(
        r'LYRA_ADS_PP_TOGGLE frame=(\d+) raw=(\d+)', Path(capture['log']).read_text())]
    assert len(events) == 2 and [raw for frame, raw in events] == [1, 0]
    raw_max = np.zeros(3)
    toggle_max = np.zeros(3)
    for sample in raw:
        frame = sample['frame']
        raw_pose = {n: transform(v) for n, v in sample['bones'].items()}
        native_raw = load_pose(runs / a.raw_run / 'poses.csv', frame)
        raw_max = np.maximum(raw_max, errors(native_raw, raw_pose))
        is_raw = 0
        for first, mode in events:
            if frame >= first:
                is_raw = mode
        # Both sides must use the same native evaluation path. The separate
        # extraction comparison remains explicit, not folded into a tolerance.
        expected = native_raw if is_raw else load_pose(original / 'poses.csv', frame)
        toggle_max = np.maximum(toggle_max, errors(load_pose(toggle / 'poses.csv', frame), expected))
    result = {'frames': len(raw), 'toggle_events': events,
              'error_units': ['cm', 'degrees', 'scale'], 'raw_replay_vs_clip_max': raw_max.tolist(),
              'toggle_vs_expected_mode_max': toggle_max.tolist(), 'passed': bool(
                  (toggle_max < [1e-3, 1e-3, 1e-5]).all()),
              'raw_extraction_is_identical_to_native_replay': bool((raw_max < [1e-3, 1e-3, 1e-5]).all())}
    (source / 'toggle-audit.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))
    assert result['passed'], 'Native source toggle/recovery differs; do not claim faithful A/B'


if __name__ == '__main__':
    main()

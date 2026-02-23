import re, json

def fix_and_parse(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Truncate at the playwright code block that follows the JSON
    cutoff = content.find('\n### Ran Playwright code')
    if cutoff > 0:
        content = content[:cutoff]
    
    # Fix playwright-cli line wrapping: long string values get split across lines
    # producing: "value"\n",  =>  should be: "value",
    import re
    content = re.sub(r'"\n"([,}\]])', r'"\1', content)
    
    # Find JSON array bounds
    start = content.index('[')
    end = content.rindex(']') + 1
    raw = content[start:end]
    
    return json.loads(raw, strict=False)

b1 = fix_and_parse(r'C:\Users\bmile\AppData\Local\Temp\copilot-tool-output-1771873581477-2pnvsz.txt')
print(f'Batch 1: {len(b1)} maps')

b2 = fix_and_parse(r'C:\Users\bmile\AppData\Local\Temp\copilot-tool-output-1771873592582-vxmi96.txt')
print(f'Batch 2: {len(b2)} maps')

b3 = fix_and_parse(r'C:\Users\bmile\AppData\Local\Temp\copilot-tool-output-1771873604793-ro2tiv.txt')
print(f'Batch 3: {len(b3)} maps')

all_maps = b1 + b2 + b3
print(f'Total: {len(all_maps)} maps')

out_path = r'C:\Users\bmile\.copilot\session-state\4c6df0b2-934f-4aad-ba4f-9e7ecb76e845\files\bakkesplugins-maps-full.json'
with open(out_path, 'w', encoding='utf-8') as f:
    json.dump(all_maps, f, indent=2, ensure_ascii=False)
print(f'Saved to {out_path}')

# Print summary stats
tags_seen = {}
for m in all_maps:
    for t in (m.get('tags') or []):
        tags_seen[t] = tags_seen.get(t, 0) + 1

print(f'\nTag distribution:')
for k, v in sorted(tags_seen.items(), key=lambda x: -x[1]):
    print(f'  {k}: {v}')

total_size = sum((f.get('fileSizeBytes') or 0) for m in all_maps for f in (m.get('files') or []))
print(f'\nTotal download size across all maps: {total_size / 1e9:.2f} GB')

maps_with_screenshots = [m for m in all_maps if m.get('screenshotUrls')]
print(f'Maps with screenshots: {len(maps_with_screenshots)}')
maps_with_files = [m for m in all_maps if m.get('files')]
print(f'Maps with download files: {len(maps_with_files)}')

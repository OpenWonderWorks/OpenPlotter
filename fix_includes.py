import os
import re

src_dir = r'i:\OpenPlotter\OpenPlotter\src'
top_folders = ['comms', 'gcode', 'hal', 'homing', 'motion', 'system', 'tools', 'utils']

for root, dirs, files in os.walk(src_dir):
    for file in files:
        if file.endswith('.h') or file.endswith('.cpp'):
            path = os.path.join(root, file)
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            depth = len(os.path.relpath(root, src_dir).split(os.sep))
            if os.path.relpath(root, src_dir) == '.':
                depth = 0
                
            prefix = '../' * depth if depth > 0 else ''
            
            def repl(match):
                inc_path = match.group(1)
                if any(inc_path.startswith(f + '/') for f in top_folders):
                    return f'#include "{prefix}{inc_path}"'
                return match.group(0)
                
            new_content = re.sub(r'#include\s+"([^"]+)"', repl, content)
            
            if new_content != content:
                with open(path, 'w', encoding='utf-8') as f:
                    f.write(new_content)
                print(f'Fixed {path}')

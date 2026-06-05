import os
import re

root_dir = 'src'
for subdir, dirs, files in os.walk(root_dir):
    for file in files:
        if file.endswith('.cpp') or file.endswith('.h'):
            filepath = os.path.join(subdir, file)
            # Calculate depth from src/
            # e.g., src/comms/serial_comms.cpp -> subdir is src/comms, depth is 2 (from OpenPlotter root)
            depth = len(subdir.split(os.sep))
            # If depth=1 (src), path is '../openplotter_config.h'
            # If depth=2 (src/comms), path is '../../openplotter_config.h'
            # If depth=3 (src/hal/pins), path is '../../../openplotter_config.h'
            rel_path = '../' * depth + 'openplotter_config.h'
            
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            
            content = re.sub(r'#include\s+"(?:(?:\.\./)+)*openplotter_config\.h"', f'#include "{rel_path}"', content)
            
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(content)

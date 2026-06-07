/**
 * OpenPlotter — Unified Log Manager
 * Centralized logging with categories, filtering, search, and export.
 * Drives both the console terminal and the dedicated log panel.
 */

// Log categories
export const LOG_CATEGORY = {
  SERIAL: 'SERIAL',
  GCODE: 'GCODE',
  FLASH: 'FLASH',
  SYSTEM: 'SYSTEM',
  ERROR: 'ERROR',
  CMD: 'CMD'
};

// Log levels
export const LOG_LEVEL = {
  DEBUG: 0,
  INFO: 1,
  WARN: 2,
  ERROR: 3
};

const LEVEL_LABELS = {
  [LOG_LEVEL.DEBUG]: 'DBG',
  [LOG_LEVEL.INFO]: 'INF',
  [LOG_LEVEL.WARN]: 'WRN',
  [LOG_LEVEL.ERROR]: 'ERR'
};

const CATEGORY_COLORS = {
  [LOG_CATEGORY.SERIAL]: '#34d399',
  [LOG_CATEGORY.GCODE]: '#60a5fa',
  [LOG_CATEGORY.FLASH]: '#c084fc',
  [LOG_CATEGORY.SYSTEM]: '#94a3b8',
  [LOG_CATEGORY.ERROR]: '#f87171',
  [LOG_CATEGORY.CMD]: '#fbbf24'
};

const CATEGORY_ICONS = {
  [LOG_CATEGORY.SERIAL]: '⇌',
  [LOG_CATEGORY.GCODE]: '◇',
  [LOG_CATEGORY.FLASH]: '⚡',
  [LOG_CATEGORY.SYSTEM]: '⚙',
  [LOG_CATEGORY.ERROR]: '✕',
  [LOG_CATEGORY.CMD]: '▶'
};

class LogManager {
  constructor(maxEntries = 5000) {
    this.entries = [];
    this.maxEntries = maxEntries;
    this.listeners = [];
    this.filters = {
      categories: new Set(Object.values(LOG_CATEGORY)),
      minLevel: LOG_LEVEL.DEBUG,
      searchQuery: ''
    };
    this.autoScroll = true;
    this.logPanel = null;
    this.logContent = null;
    this.lineCountEl = null;
    this.systemLogContainer = null;
    this._filterBtns = {};
    this._bound = false;
  }

  /**
   * Add a log entry
   */
  log(category, level, message, data = null) {
    const entry = {
      id: this.entries.length,
      timestamp: new Date(),
      category,
      level,
      message: typeof message === 'string' ? message : String(message),
      data
    };

    this.entries.push(entry);

    // Circular buffer trim
    if (this.entries.length > this.maxEntries) {
      this.entries = this.entries.slice(-Math.floor(this.maxEntries * 0.8));
    }

    // Notify listeners
    this.listeners.forEach(fn => {
      try { fn(entry); } catch (e) { console.error('Log listener error:', e); }
    });

    // Render to log panel
    this._renderEntry(entry);

    return entry;
  }

  // Convenience methods
  serial(message, direction = 'rx') {
    const prefix = direction === 'tx' ? '→ ' : '← ';
    return this.log(LOG_CATEGORY.SERIAL, LOG_LEVEL.INFO, prefix + message);
  }

  gcode(message) {
    return this.log(LOG_CATEGORY.GCODE, LOG_LEVEL.INFO, message);
  }

  flash(message) {
    return this.log(LOG_CATEGORY.FLASH, LOG_LEVEL.INFO, message);
  }

  system(message) {
    return this.log(LOG_CATEGORY.SYSTEM, LOG_LEVEL.INFO, message);
  }

  error(message, err = null) {
    const fullMessage = err ? `${message}: ${err.message || err}` : message;
    return this.log(LOG_CATEGORY.ERROR, LOG_LEVEL.ERROR, fullMessage, err);
  }

  warn(message) {
    return this.log(LOG_CATEGORY.SYSTEM, LOG_LEVEL.WARN, message);
  }

  cmd(message) {
    return this.log(LOG_CATEGORY.CMD, LOG_LEVEL.INFO, message);
  }

  /**
   * Register a listener for new log entries
   */
  onEntry(fn) {
    this.listeners.push(fn);
    return () => {
      this.listeners = this.listeners.filter(f => f !== fn);
    };
  }

  /**
   * Get filtered entries
   */
  getFiltered() {
    return this.entries.filter(entry => {
      if (!this.filters.categories.has(entry.category)) return false;
      if (entry.level < this.filters.minLevel) return false;
      if (this.filters.searchQuery) {
        const q = this.filters.searchQuery.toLowerCase();
        if (!entry.message.toLowerCase().includes(q)) return false;
      }
      return true;
    });
  }

  /**
   * Toggle a category filter
   */
  toggleCategory(category) {
    if (this.filters.categories.has(category)) {
      this.filters.categories.delete(category);
    } else {
      this.filters.categories.add(category);
    }
    this._refreshPanel();
    this._updateFilterButtons();
  }

  /**
   * Set search query
   */
  setSearch(query) {
    this.filters.searchQuery = query;
    this._refreshPanel();
  }

  /**
   * Export all entries to a text file
   */
  exportToFile() {
    const lines = this.entries.map(e => {
      const ts = e.timestamp.toISOString();
      const lvl = LEVEL_LABELS[e.level] || 'INF';
      return `[${ts}] [${e.category}] [${lvl}] ${e.message}`;
    });

    const blob = new Blob([lines.join('\n')], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `openplotter_log_${new Date().toISOString().slice(0, 10)}.log`;
    a.click();
    URL.revokeObjectURL(url);
  }

  /**
   * Clear all log entries
   */
  clear() {
    this.entries = [];
    if (this.logContent) {
      this.logContent.innerHTML = '';
    }
    if (this.systemLogContainer) {
      this.systemLogContainer.textContent = '';
    }
    this._updateLineCount();
  }

  /**
   * Bind the log manager to DOM elements
   */
  bind(panelEl, contentEl, lineCountEl, systemLogContainer) {
    this.logPanel = panelEl;
    this.logContent = contentEl;
    this.lineCountEl = lineCountEl;
    this.systemLogContainer = systemLogContainer;
    this._bound = true;
  }

  /**
   * Format a timestamp for display
   */
  _formatTime(date) {
    const h = String(date.getHours()).padStart(2, '0');
    const m = String(date.getMinutes()).padStart(2, '0');
    const s = String(date.getSeconds()).padStart(2, '0');
    const ms = String(date.getMilliseconds()).padStart(3, '0');
    return `${h}:${m}:${s}.${ms}`;
  }

  /**
   * Render a single entry to the log panel
   */
  _renderEntry(entry) {
    if (!this.logContent) return;

    // Check filters
    if (!this.filters.categories.has(entry.category)) return;
    if (entry.level < this.filters.minLevel) return;
    if (this.filters.searchQuery) {
      const q = this.filters.searchQuery.toLowerCase();
      if (!entry.message.toLowerCase().includes(q)) return;
    }

    const line = document.createElement('div');
    line.className = `log-line log-${entry.category.toLowerCase()} log-level-${entry.level}`;
    line.dataset.id = entry.id;

    const color = CATEGORY_COLORS[entry.category] || '#94a3b8';
    const icon = CATEGORY_ICONS[entry.category] || '•';

    line.innerHTML = `<span class="log-time">${this._formatTime(entry.timestamp)}</span>`
      + `<span class="log-cat" style="color:${color}" title="${entry.category}">${icon}</span>`
      + `<span class="log-msg">${this._escapeHtml(entry.message)}</span>`;

    this.logContent.appendChild(line);
    
    // Also append to system logs container if available
    if (this.systemLogContainer) {
      const sysLvl = LEVEL_LABELS[entry.level] || 'INF';
      this.systemLogContainer.textContent += `[${this._formatTime(entry.timestamp)}] [${entry.category}] [${sysLvl}] ${entry.message}\n`;
    }

    // Trim DOM nodes if too many
    while (this.logContent.children.length > 1000) {
      this.logContent.removeChild(this.logContent.firstChild);
    }

    // Auto-scroll
    if (this.autoScroll) {
      this.logContent.scrollTop = this.logContent.scrollHeight;
      if (this.systemLogContainer) {
        this.systemLogContainer.scrollTop = this.systemLogContainer.scrollHeight;
      }
    }

    this._updateLineCount();
  }

  /**
   * Full refresh of the log panel (after filter change)
   */
  _refreshPanel() {
    if (!this.logContent) return;
    this.logContent.innerHTML = '';

    const filtered = this.getFiltered();
    // Only render last 500 for performance
    const toRender = filtered.slice(-500);
    
    const fragment = document.createDocumentFragment();
    toRender.forEach(entry => {
      const color = CATEGORY_COLORS[entry.category] || '#94a3b8';
      const icon = CATEGORY_ICONS[entry.category] || '•';

      const line = document.createElement('div');
      line.className = `log-line log-${entry.category.toLowerCase()} log-level-${entry.level}`;
      line.dataset.id = entry.id;
      line.innerHTML = `<span class="log-time">${this._formatTime(entry.timestamp)}</span>`
        + `<span class="log-cat" style="color:${color}" title="${entry.category}">${icon}</span>`
        + `<span class="log-msg">${this._escapeHtml(entry.message)}</span>`;
      fragment.appendChild(line);
    });

    this.logContent.appendChild(fragment);

    if (this.autoScroll) {
      this.logContent.scrollTop = this.logContent.scrollHeight;
    }
    this._updateLineCount();
  }

  _updateLineCount() {
    if (this.lineCountEl) {
      const filtered = this.getFiltered().length;
      const total = this.entries.length;
      this.lineCountEl.textContent = filtered === total
        ? `${total} lines`
        : `${filtered}/${total} lines`;
    }
  }

  _updateFilterButtons() {
    Object.entries(this._filterBtns).forEach(([cat, btn]) => {
      if (this.filters.categories.has(cat)) {
        btn.classList.add('active');
      } else {
        btn.classList.remove('active');
      }
    });
  }

  _escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  }
}

// Singleton
export const logManager = new LogManager();

import * as tauriLog from '@tauri-apps/plugin-log';

/**
 * 格式化额外信息，确保换行符正确显示
 */
function formatExtraInfo(extra: unknown): string {
    if (!extra) return '';

    if (typeof extra === 'string') {
        return extra;
    }

    if (typeof extra === 'object') {
        try {
            const obj = extra as Record<string, unknown>;
            const lines: string[] = [];

            for (const [key, value] of Object.entries(obj)) {
                if (key === 'stack' && typeof value === 'string') {
                    // 堆栈信息直接输出，保留换行符
                    lines.push(`${key}:`);
                    lines.push(value);
                } else if (typeof value === 'string' && value.includes('\n')) {
                    // 其他包含换行符的字符串
                    lines.push(`${key}:`);
                    lines.push(value);
                } else {
                    // 其他值使用 JSON.stringify
                    lines.push(`${key}: ${JSON.stringify(value)}`);
                }
            }

            return '\n' + lines.join('\n');
        } catch {
            return '\n[无法序列化额外信息]';
        }
    }

    return String(extra);
}

export function appError(message: string, extra?: unknown, options?: tauriLog.LogOptions) {
    const extraInfo = formatExtraInfo(extra);
    tauriLog.error(`[${location.href}] ${message}${extraInfo}`, options);
}

export function appWarn(message: string, extra?: unknown, options?: tauriLog.LogOptions) {
    const extraInfo = formatExtraInfo(extra);
    tauriLog.warn(`[${location.href}] ${message}${extraInfo}`, options);
}

export function appInfo(message: string, extra?: unknown, options?: tauriLog.LogOptions) {
    const extraInfo = formatExtraInfo(extra);
    tauriLog.info(`[${location.href}] ${message}${extraInfo}`, options);
}

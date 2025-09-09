import { ElementRect } from '@/commands';
import { getAppConfigBaseDir } from '@/commands/file';
import { BubbleDataType } from '@ant-design/x/es/bubble/BubbleList';
import { Conversation } from '@ant-design/x/es/conversations';
import { MessageInfo } from '@ant-design/x/es/use-x-chat';
import { Ordered } from '@mg-chao/excalidraw/element/types';
import { NonDeletedExcalidrawElement } from '@mg-chao/excalidraw/element/types';
import { AppState } from '@mg-chao/excalidraw/types';
import { load, Store } from '@tauri-apps/plugin-store';

class BaseStore<Value> {
    protected instance: Store | undefined;
    protected name: string;
    protected autoSave: number;

    constructor(name: string, autoSave: number = 1000) {
        this.name = name;
        this.autoSave = autoSave;
    }

    public async init() {
        this.instance = await load(`${await getAppConfigBaseDir()}/stores/${this.name}.json`, {
            autoSave: this.autoSave,
        });
    }

    public inited() {
        return !!this.instance;
    }

    public async set(key: string, value: Value) {
        if (!this.instance) {
            throw new Error('Store not initialized');
        }

        return await this.instance.set(key, value);
    }

    public async get(key: string): Promise<Value | undefined> {
        if (!this.instance) {
            throw new Error('Store not initialized');
        }

        return await this.instance.get<Value>(key);
    }

    public async entries(): Promise<[key: string, value: Value][]> {
        if (!this.instance) {
            throw new Error('Store not initialized');
        }

        return await this.instance.entries<Value>();
    }

    public async clear() {
        if (!this.instance) {
            throw new Error('Store not initialized');
        }

        return await this.instance.clear();
    }

    public async delete(key: string) {
        if (!this.instance) {
            throw new Error('Store not initialized');
        }

        return await this.instance.delete(key);
    }

    public async save() {
        if (!this.instance) {
            throw new Error('Store not initialized');
        }

        return await this.instance.save();
    }
}

export class ChatHistoryStore extends BaseStore<{
    session: Conversation;
    messages: MessageInfo<BubbleDataType>[];
}> {
    constructor() {
        super('chat-history', 1000);
    }
}

export class ExcalidrawAppStateStore extends BaseStore<{
    appState: Partial<AppState>;
}> {
    constructor() {
        super('excalidraw-app-state', 0);
    }
}

export type ChatWorkflowFlow = {
    variable_name?: string;
    ignore_context: boolean;
    message: string;
};

export type ChatWorkflowConfig = {
    id: string;
    name: string;
    description?: string;
    flow_list: ChatWorkflowFlow[];
};

export class ChatWorkflowConfigStore extends BaseStore<ChatWorkflowConfig> {
    constructor() {
        super('chat-workflow-config', 0);
    }
}

export type CaptureHistoryItem = {
    id: string;
    selected_rect: ElementRect;
    file_name: string;
    excalidraw_elements: readonly Ordered<NonDeletedExcalidrawElement>[] | undefined;
    excalidraw_app_state: Pick<AppState, keyof AppState> | undefined;
    create_ts: number;
};

export class CaptureHistoryStore extends BaseStore<CaptureHistoryItem> {
    constructor() {
        super('capture-history', 0);
    }
}

export const clearAllAppStore = async () => {
    // 忽略截图历史的存储，因为和图片文件关联
    // const chatHistoryStore = new ChatHistoryStore();
    const excalidrawAppStateStore = new ExcalidrawAppStateStore();
    const chatWorkflowConfigStore = new ChatWorkflowConfigStore();

    await Promise.all([
        // chatHistoryStore.init(),
        excalidrawAppStateStore.init(),
        chatWorkflowConfigStore.init(),
    ]);

    await Promise.all([
        // chatHistoryStore.clear(),
        excalidrawAppStateStore.clear(),
        chatWorkflowConfigStore.clear(),
    ]);
};

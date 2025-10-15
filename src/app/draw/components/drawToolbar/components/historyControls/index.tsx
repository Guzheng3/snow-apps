import { DrawToolbarKeyEventKey } from '@/types/components/drawToolbar';
import { RedoOutlined, UndoOutlined } from '@ant-design/icons';
import { useEffect, useState } from 'react';
import React from 'react';
import { DrawState } from '@/types/draw';
import { ToolButton } from '../toolButton';
import { useHistory } from '@/app/fullScreenDraw/components/drawCore/components/historyContext';

const HistoryControlsCore: React.FC<{ disable: boolean; hidden?: boolean }> = ({
    disable,
    hidden,
}) => {
    const { history } = useHistory();
    const [canUndo, setCanUndo] = useState(false);
    const [canRedo, setCanRedo] = useState(false);

    useEffect(() => {
        const unlisten = history.addOnUpdateListener(() => {
            setCanUndo(history.canUndo());
            setCanRedo(history.canRedo());
        });

        return () => {
            unlisten();
        };
    }, [history]);

    return (
        <>
            {/* 撤销 */}
            <ToolButton
                hidden={hidden}
                componentKey={DrawToolbarKeyEventKey.UndoTool}
                icon={<UndoOutlined />}
                drawState={DrawState.Undo}
                disable={!canUndo || disable}
                onClick={() => {
                    history.undo();
                }}
            />

            {/* 重做 */}
            <ToolButton
                hidden={hidden}
                componentKey={DrawToolbarKeyEventKey.RedoTool}
                icon={<RedoOutlined />}
                drawState={DrawState.Redo}
                disable={!canRedo || disable}
                onClick={() => {
                    history.redo();
                }}
            />
        </>
    );
};

export const HistoryControls = React.memo(HistoryControlsCore);

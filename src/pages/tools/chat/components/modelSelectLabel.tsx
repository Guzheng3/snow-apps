import { Space, Tag } from "antd";
import { FormattedMessage } from "react-intl";

export const ModelSelectLabel: React.FC<{
	modelName: string;
	custom?: boolean;
	reasoner?: boolean;
}> = ({ modelName, custom, reasoner }) => {
	return (
		<Space>
			<div>{modelName}</div>
			<div>
				{custom && (
					<Tag color="green">
						<FormattedMessage id="tools.chat.custom" />
					</Tag>
				)}
				{reasoner && (
					<Tag color="processing">
						<FormattedMessage id="tools.chat.reasoner" />
					</Tag>
				)}
			</div>
		</Space>
	);
};

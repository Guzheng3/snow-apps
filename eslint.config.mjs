import { dirname } from 'path';
import { fileURLToPath } from 'url';
import { FlatCompat } from '@eslint/eslintrc';
import compat from 'eslint-plugin-compat';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

const compatConfig = new FlatCompat({
    baseDirectory: __dirname,
});

const eslintConfig = [{
    ignores: ["node_modules/**", ".next/**", "out/**", "build/**", "next-env.d.ts", "src-tauri/**"]
}, ...compatConfig.extends('next/core-web-vitals', 'next/typescript', 'prettier'), {
    plugins: {
        compat,
    },
    rules: {
        '@typescript-eslint/no-duplicate-enum-values': 'off',
        '@typescript-eslint/no-empty-object-type': 'off',
        'compat/compat': 'error',
    },
    ignores: ['./src/global.d.ts'],
}];

export default eslintConfig;

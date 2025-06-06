// Copyright (c) 2022-2023 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

import assert from 'node:assert';
import path from 'node:path';
import { StructureGroups } from '@workerd/jsg/rtti';
import ts from 'typescript';
import { getTypeName } from '../../generator';
import { SourcesMap } from '../../program';

// If an override matches this RegExp, it will replace the existing definition
const keywordReplace =
  /^export |^declare |^type |^abstract |^class |^interface |^enum |^const |^var |^function /;
// If an override matches this RegExp, it will have `class ${name} ` prefixed
const keywordHeritage = /^extends |^implements /;

function compileOverride(
  name: string,
  override: string
): [compiled: string, isReplacement: boolean] {
  // If this override is a complete type replacement, return it as is
  // Examples:
  // - `const WebSocketPair: { new (): { 0: WebSocket; 1: WebSocket }; }`
  // - `type ReadableStreamReadResult<R = any> = { done: false, value: R; } | { done: true; value?: undefined; }`
  // - `type TransactionOptions = never` (deletes definition)
  if (keywordReplace.test(override)) return [override, true];

  // Fix up overrides, so they can be parsed as TypeScript source files. Whilst
  // we convert all overrides to classes, this type classification is ignored
  // when merging. Classes just support all possible forms of override (extends,
  // implements, constructors, (static) properties/methods).
  if (keywordHeritage.test(override) || override.startsWith('{')) {
    // Use existing name and type classification, may merge members
    // Examples:
    // - `extends EventTarget<WorkerGlobalScopeEventMap>`
    // - `extends TransformStream<ArrayBuffer | ArrayBufferView, Uint8Array> { constructor(format: "gzip" | "deflate" | "deflate-raw"); }`
    // - `{ json<T>(): Promise<T>; }`
    override = `class ${name} ${override}`;
  } else if (override.startsWith('<')) {
    // Use existing name and type classification, may merge members
    // Examples:
    // - `<R = any> { read(): Promise<ReadableStreamReadResult<R>>; }`
    override = `class ${name}${override}`;
  } else {
    // Use existing type classification, may rename and merge members
    // Examples:
    // - `KVNamespaceGetOptions<Type> { type: Type; }`
    // - `KVNamespaceListOptions` (just rename definition)
    // - `WorkerGlobalScope extends EventTarget<WorkerGlobalScopeEventMap>`
    override = `class ${override}`;
  }
  // Purely heritage and rename overrides don't need to define any members, but
  // they still need to be valid classes for parsing.
  if (!override.endsWith('}')) {
    override = `${override} {}`;
  }

  return [override, false];
}

const overridesPath = '/$virtual/overrides';
const definesPath = '/$virtual/defines';

// Converts and collects all overrides and defines as TypeScript source files.
// Also returns a set of definitions that should be replaced by their override.
export function compileOverridesDefines(
  root: StructureGroups
): [sources: SourcesMap, replacements: Set<string>] {
  const sources = new SourcesMap();
  // Types that need their definition completely replaced by their override
  const replacements = new Set<string>();

  root.groups.forEach((group) => {
    group.structures.forEach((structure) => {
      const name = getTypeName(structure);
      const override = structure.tsOverride.trim();
      const define = structure.tsDefine.trim();

      if (override !== '') {
        const [compiled, isReplacement] = compileOverride(name, override);
        sources.set(path.join(overridesPath, name + '.ts'), compiled);
        if (isReplacement) replacements.add(name);
      }
      if (define !== '') {
        sources.set(path.join(definesPath, name + '.ts'), define);
      }
    });
  });

  return [sources, replacements];
}

// Try to find override for structure using name obtained from `getTypeName()`
export function maybeGetOverride(
  program: ts.Program,
  name: string
): ts.Statement | undefined {
  const sourcePath = path.join(overridesPath, name + '.ts');
  const sourceFile = program.getSourceFile(sourcePath);
  if (sourceFile !== undefined) {
    assert.strictEqual(sourceFile.statements.length, 1);
    return sourceFile.statements[0];
  }
}

// Try to find defines for structure using name obtained from `getTypeName()`
export function maybeGetDefines(
  program: ts.Program,
  name: string
): ts.NodeArray<ts.Statement> | undefined {
  const sourcePath = path.join(definesPath, name + '.ts');
  const sourceFile = program.getSourceFile(sourcePath);
  return sourceFile?.statements;
}

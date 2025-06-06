// Copyright (c) 2023 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0

import * as assert from 'node:assert';

import { itShould, testD1ApiQueriesHappyPath } from './d1-api-test-common';

const test = (fn, getDB = getDBFromEnv) => ({
  async test(ctr, env) {
    await fn(getDB(env), env.d1MockFetcher);
  },
});

function getDBFromEnv(env) {
  return env.d1;
}

export const test_d1_api_happy_path = test(
  testD1ApiQueriesHappyPath,
  getDBFromEnv
);

export const test_d1_api_happy_path_withsessions_default = test(
  testD1ApiQueriesHappyPath,
  (env) => getDBFromEnv(env).withSession()
);

export const test_d1_api_happy_path_withsessions_first_unconstrained = test(
  testD1ApiQueriesHappyPath,
  (env) => getDBFromEnv(env).withSession('first-unconstrained')
);

export const test_d1_api_happy_path_withsessions_first_primary = test(
  testD1ApiQueriesHappyPath,
  (env) => getDBFromEnv(env).withSession('first-primary')
);

export const test_d1_api_happy_path_withsessions_some_ranomd_token = test(
  testD1ApiQueriesHappyPath,
  (env) => getDBFromEnv(env).withSession('token-doesnot-matter-for-now')
);

// envD1MockFetcher is the default export worker in `d1-mock.js`, i.e. the `fetch()` entry point.
const getCommitTokensSentFromBinding = async (envD1MockFetcher) =>
  (
    await (
      await envD1MockFetcher.fetch(`http://d1-api-test/commitTokens`)
    ).json()
  ).commitTokensReceived;
const getCommitTokensReturnedFromEyeball = async (envD1MockFetcher) =>
  (
    await (
      await envD1MockFetcher.fetch(`http://d1-api-test/commitTokens`)
    ).json()
  ).commitTokensReturned;
const resetCommitTokens = async (envD1MockFetcher) =>
  await (
    await envD1MockFetcher.fetch(`http://d1-api-test/commitTokens/reset`)
  ).json();
const setNextCommitTokenFromEyeball = async (envD1MockFetcher, t) =>
  await (
    await envD1MockFetcher.fetch(
      `http://d1-api-test/commitTokens/nextToken?t=${t}`
    )
  ).json();

export const test_d1_api_withsessions_token_handling = test(
  testD1ApiWithSessionsTokensHandling,
  getDBFromEnv
);

async function testD1ApiWithSessionsTokensHandling(DB, envD1MockFetcher) {
  const assertTokensSentReceived = async (firstTokenFromBinding) => {
    const tokens = await getCommitTokensSentFromBinding(envD1MockFetcher);
    assert.deepEqual(tokens[0], firstTokenFromBinding);
    // Make sure we sent back whatever we received from the previous query.
    assert.deepEqual(
      tokens.slice(1),
      (await getCommitTokensReturnedFromEyeball(envD1MockFetcher)).slice(0, -1)
    );
  };

  // Assert tokens sent by the top level DB are always primary!
  await resetCommitTokens(envD1MockFetcher);
  await testD1ApiQueriesHappyPath(DB);
  let tokens = await getCommitTokensSentFromBinding(envD1MockFetcher);
  assert.deepEqual(
    tokens.every((t) => t === 'first-primary'),
    true
  );
  // Make sure we received different tokens, and still sent first-primary.
  assert.deepEqual(
    (await getCommitTokensReturnedFromEyeball(envD1MockFetcher)).every(
      (t) => t !== 'first-primary'
    ),
    true
  );

  // Assert tokens sent by the DEFAULT DB.withSession()
  await resetCommitTokens(envD1MockFetcher);
  await testD1ApiQueriesHappyPath(DB.withSession());
  await assertTokensSentReceived('first-unconstrained');

  // Assert tokens sent by the DB.withSession("first-unconstrained")
  await resetCommitTokens(envD1MockFetcher);
  await testD1ApiQueriesHappyPath(DB.withSession('first-unconstrained'));
  await assertTokensSentReceived('first-unconstrained');

  // Assert tokens sent by the DB.withSession("first-primary")
  await resetCommitTokens(envD1MockFetcher);
  await testD1ApiQueriesHappyPath(DB.withSession('first-primary'));
  await assertTokensSentReceived('first-primary');
}

export const test_d1_api_withsessions_old_token_skipped = test(
  testD1ApiWithSessionsOldTokensSkipped,
  getDBFromEnv
);

async function testD1ApiWithSessionsOldTokensSkipped(DB, envD1MockFetcher) {
  const runTest = async (session) => {
    await resetCommitTokens(envD1MockFetcher);
    await session.prepare(`SELECT * FROM sqlite_master;`).all();
    await session.prepare(`SELECT * FROM sqlite_master;`).all();

    // This is alphanumerically smaller than the tokens generated by d1-mock.js.
    await setNextCommitTokenFromEyeball(envD1MockFetcher, '------');

    // The token from this should be ignored!
    await session.prepare(`SELECT * FROM sqlite_master;`).all();

    // But not from this since a normal larger value should be received.
    await session.prepare(`SELECT * FROM sqlite_master;`).all();
    await session.prepare(`SELECT * FROM sqlite_master;`).all();

    const tokensFromBinding =
      await getCommitTokensSentFromBinding(envD1MockFetcher);
    const tokensFromEyeball =
      await getCommitTokensReturnedFromEyeball(envD1MockFetcher);
    const expectedTokensFromBinding = [
      'first-unconstrained',
      tokensFromEyeball[0],
      tokensFromEyeball[1],
      // We skip the commit token "------", since the previously received one was more recent.
      tokensFromEyeball[1],
      // The binding then sends back the next largest value.
      tokensFromEyeball[3],
    ];
    assert.deepEqual(tokensFromBinding, expectedTokensFromBinding);

    return { ok: true };
  };

  itShould('default DB', runTest(DB), { ok: true });
  itShould('withSession()', runTest(DB.withSession()), { ok: true });
  itShould(
    'withSession(first-unconstrained)',
    runTest(DB.withSession('first-unconstrained')),
    { ok: true }
  );
  itShould(
    'withSession(first-primary)',
    runTest(DB.withSession('first-primary')),
    { ok: true }
  );
}

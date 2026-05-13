// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import { BraveWallet } from '../constants/types'

// Utils
import {
  hasPermitShape,
  isPermitLikeEthSignTypedData,
  PERMIT_LIKE,
} from './sign_utils'

function makeEthSignTypedData(
  overrides: Partial<BraveWallet.EthSignTypedData>,
): BraveWallet.EthSignTypedData {
  return {
    addressParam: '0x',
    chainId: '1',
    domainHash: [],
    domainJson: '{}',
    messageJson: '{}',
    meta: undefined,
    primaryHash: [],
    primaryType: 'Person',
    typesJson: '{}',
    ...overrides,
  }
}

function typesJsonForPrimary(
  primaryType: string,
  fields: { name: string; type: string }[],
): string {
  return JSON.stringify({ [primaryType]: fields })
}

describe('hasPermitShape', () => {
  it('returns false when typesJson is empty', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'Foo',
          typesJson: '',
        }),
      ),
    ).toBe(false)
  })

  it('returns false when typesJson is invalid JSON', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'Foo',
          typesJson: '{',
        }),
      ),
    ).toBe(false)
  })

  it('returns false when primary type is missing from types', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'Missing',
          typesJson: typesJsonForPrimary('Other', [
            { name: 'spender', type: 'address' },
            { name: 'value', type: 'uint256' },
          ]),
        }),
      ),
    ).toBe(false)
  })

  it('returns false when spender is missing', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'Foo',
          typesJson: typesJsonForPrimary('Foo', [
            { name: 'owner', type: 'address' },
            { name: 'value', type: 'uint256' },
          ]),
        }),
      ),
    ).toBe(false)
  })

  it('returns false when spender exists but no value, amount, or permit nested types', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'Foo',
          typesJson: typesJsonForPrimary('Foo', [
            { name: 'spender', type: 'address' },
            { name: 'owner', type: 'address' },
          ]),
        }),
      ),
    ).toBe(false)
  })

  it('returns true for spender and value', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'CustomPermit',
          typesJson: typesJsonForPrimary('CustomPermit', [
            { name: 'owner', type: 'address' },
            { name: 'spender', type: 'address' },
            { name: 'value', type: 'uint256' },
          ]),
        }),
      ),
    ).toBe(true)
  })

  it('returns true for spender and amount', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'CustomPermit',
          typesJson: typesJsonForPrimary('CustomPermit', [
            { name: 'spender', type: 'address' },
            { name: 'amount', type: 'uint256' },
          ]),
        }),
      ),
    ).toBe(true)
  })

  it('returns true for spender and PermitDetails field', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'PermitSingle',
          typesJson: typesJsonForPrimary('PermitSingle', [
            { name: 'details', type: 'PermitDetails' },
            { name: 'spender', type: 'address' },
            { name: 'sigDeadline', type: 'uint256' },
          ]),
        }),
      ),
    ).toBe(true)
  })

  it('returns true for spender and PermitDetails[] field', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'PermitBatch',
          typesJson: typesJsonForPrimary('PermitBatch', [
            { name: 'details', type: 'PermitDetails[]' },
            { name: 'spender', type: 'address' },
          ]),
        }),
      ),
    ).toBe(true)
  })

  it('returns true for spender and TokenPermissions field', () => {
    expect(
      hasPermitShape(
        makeEthSignTypedData({
          primaryType: 'PermitTransferFrom',
          typesJson: typesJsonForPrimary('PermitTransferFrom', [
            { name: 'permitted', type: 'TokenPermissions' },
            { name: 'spender', type: 'address' },
          ]),
        }),
      ),
    ).toBe(true)
  })
})

describe('isPermitLikeEthSignTypedData', () => {
  it('returns false for undefined', () => {
    expect(isPermitLikeEthSignTypedData(undefined)).toBe(false)
  })

  it.each(PERMIT_LIKE)('returns true when primaryType is %s', (primaryType) => {
    expect(
      isPermitLikeEthSignTypedData(
        makeEthSignTypedData({
          primaryType,
          typesJson: '{}',
        }),
      ),
    ).toBe(true)
  })

  it('returns false for non-permit primary type and non-permit shape', () => {
    expect(
      isPermitLikeEthSignTypedData(
        makeEthSignTypedData({
          primaryType: 'Person',
          typesJson: typesJsonForPrimary('Person', [
            { name: 'name', type: 'string' },
            { name: 'wallet', type: 'address' },
          ]),
        }),
      ),
    ).toBe(false)
  })

  it('returns true for permit-shaped message with unknown primary type name', () => {
    expect(
      isPermitLikeEthSignTypedData(
        makeEthSignTypedData({
          primaryType: 'VendorCustomPermit',
          typesJson: typesJsonForPrimary('VendorCustomPermit', [
            { name: 'owner', type: 'address' },
            { name: 'spender', type: 'address' },
            { name: 'value', type: 'uint256' },
          ]),
        }),
      ),
    ).toBe(true)
  })

  it('returns false when primaryType is empty and shape does not match', () => {
    expect(
      isPermitLikeEthSignTypedData(
        makeEthSignTypedData({
          primaryType: '',
          typesJson: '{}',
        }),
      ),
    ).toBe(false)
  })
})

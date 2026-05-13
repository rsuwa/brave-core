// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

import { BraveWallet } from '../constants/types'

export const PERMIT_LIKE: readonly string[] = [
  'Permit',
  'PermitSingle',
  'PermitBatch',
  'PermitTransferFrom',
  'PermitWitnessTransferFrom',
]

type TypedDataField = { name: string; type: string }

function parseTypesJson(
  typesJson: string | undefined,
): Record<string, TypedDataField[]> | undefined {
  if (!typesJson) {
    return undefined
  }
  try {
    return JSON.parse(typesJson) as Record<string, TypedDataField[]>
  } catch {
    return undefined
  }
}

export function hasPermitShape(td: BraveWallet.EthSignTypedData): boolean {
  const types = parseTypesJson(td.typesJson)
  const primaryType = td.primaryType
  if (!types || !primaryType) {
    return false
  }
  const t = types[primaryType]
  if (!Array.isArray(t)) {
    return false
  }
  const names = new Set(t.map((f) => f.name))
  return (
    names.has('spender')
    && (names.has('value')
      || names.has('amount')
      || t.some(
        (f) =>
          f.type === 'PermitDetails'
          || f.type === 'PermitDetails[]'
          || f.type === 'TokenPermissions',
      ))
  )
}

export function isPermitLikeEthSignTypedData(
  td: BraveWallet.EthSignTypedData | undefined,
): boolean {
  if (!td) {
    return false
  }
  return (
    (!!td.primaryType && PERMIT_LIKE.includes(td.primaryType))
    || hasPermitShape(td)
  )
}

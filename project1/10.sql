SELECT CP.nickname
FROM CatchedPokemon CP
WHERE CP.owner_id >= 6 AND CP.level >= 50
ORDER BY CP.nickname ASC
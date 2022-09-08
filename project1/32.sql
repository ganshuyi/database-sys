SELECT DISTINCT P.name
FROM Pokemon P, CatchedPokemon CP, Trainer T
WHERE NOT (P.id = CP.pid AND T.id = CP.owner_id)
ORDER BY P.name ASC
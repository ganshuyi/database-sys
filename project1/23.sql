SELECT DISTINCT T.name
FROM CatchedPokemon CP, Trainer T
WHERE CP.owner_id = T.id AND CP.level <= 10
ORDER BY T.name ASC
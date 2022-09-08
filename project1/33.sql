SELECT SUM(CP.level)
FROM CatchedPokemon CP, Trainer T
WHERE T.id = CP.owner_id AND T.name = "Matis"
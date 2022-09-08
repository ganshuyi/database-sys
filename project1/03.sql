SELECT AVG(CP.level)
FROM CatchedPokemon CP, Trainer T
WHERE CP.owner_id = T.id AND T.hometown = "Sangnok City"
SELECT COUNT(DISTINCT P.type)
FROM CatchedPokemon CP, Pokemon P, Trainer T
WHERE CP.owner_id = T.id AND CP.pid = P.id AND T.hometown = "Sangnok City"
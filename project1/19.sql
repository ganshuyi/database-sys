SELECT COUNT(DISTINCT P.type)
FROM CatchedPokemon CP, Pokemon P, Trainer T, Gym G
WHERE CP.owner_id = T.id AND CP.pid = P.id AND T.id = G.leader_id AND G.city = "Sangnok City"


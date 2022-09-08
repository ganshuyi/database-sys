SELECT T.name, AVG(CP.level)
FROM CatchedPokemon CP, Trainer T, Gym G
WHERE CP.owner_id = T.id AND T.id = G.leader_id
GROUP BY T.id
SELECT T.name, COUNT(*)
FROM CatchedPokemon CP, Pokemon P, Trainer T, Gym G
WHERE CP.owner_id = T.id AND CP.pid = P.id AND T.id = G.leader_id
GROUP BY T.name
ORDER BY T.name ASC
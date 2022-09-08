SELECT P.name, CP.level, CP.nickname
FROM Pokemon P, CatchedPokemon CP, Trainer T, Gym G
WHERE P.id = CP.pid AND T.id = CP.owner_id AND G.leader_id = T.id AND CP.nickname LIKE "A%"
ORDER BY CP.nickname DESC
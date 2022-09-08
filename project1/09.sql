SELECT T.name, AVG(CP.level)
FROM CatchedPokemon CP, Trainer T
WHERE EXISTS (
  SELECT G.leader_id
  FROM Gym G
  WHERE G.leader_id = T.id AND CP.owner_id = T.id)
GROUP BY T.name
ORDER BY T.name ASC
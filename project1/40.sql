SELECT T.hometown, CP.nickname
FROM Trainer T, CatchedPokemon CP
WHERE T.id = CP.owner_id AND CP.level IN (
  SELECT MAX(CP.level)
  FROM CatchedPokemon CP, Trainer T
  WHERE CP.owner_id = T.id
  GROUP BY T.hometown)
GROUP BY T.hometown
ORDER BY T.hometown ASC
SELECT DISTINCT P.name
FROM Pokemon P, CatchedPokemon CP, Trainer T
WHERE P.id = CP.pid AND CP.owner_id = T.id AND (T.hometown = "Sangnok City" OR T.hometown = "Brown City")
ORDER BY P.name ASC
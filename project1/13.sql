SELECT P.name, P.id
FROM CatchedPokemon CP, Pokemon P, Trainer T
WHERE CP.pid = P.id AND T.id = CP.owner_id AND T.hometown = "Sangnok City"
ORDER BY P.id ASC
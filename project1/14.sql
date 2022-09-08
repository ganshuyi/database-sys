SELECT P.name
FROM Pokemon P, Evolution E
WHERE P.type = "Grass" AND P.id = E.before_id
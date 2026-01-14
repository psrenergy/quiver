final element_ids = db.getElementsIdsFromCollectionId(collection_id);
final map = db.readElementScalarAttributesFromId(collection_id, element_id);
final list = db.readElementGroupOfTimeSeriesAttributesFromId(collection_id, element_id, attribute_group_id, attribute_group_group_dimension_keys);
final list = db.readElementGroupOfVectorAttributesFromId(collection_id, element_id, attribute_group_id);          
final list = db.readElementGroupOfSetAttributesFromId(collection_id, element_id, attribute_group_id);

db.updateElementScalarAttributesFromId(collection_id, element_id, map);
db.updateElementGroupOfTimeSeriesFromId(collection_id, element_id, attribute_group_id, map);
db.updateElementGroupOfVectorAttributesFromId( collection_id, element_id, attribute_group_id, map);
db.updateElementGroupOfSetAttributesFromId(collection_id, element_id, attribute_group_id,  map);

db.createElement("Configuration", map);

db.deleteElementFromId(collection_id, element_id);

db.close();


using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;

using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Linq;
using Autodesk.Revit.ApplicationServices;
using Autodesk.Revit.Attributes;
using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Architecture;
using Autodesk.Revit.DB.IFC;
using Autodesk.Revit.UI;
using Autodesk.Revit.UI.Selection;
using Autodesk.Revit.DB.Structure;
using Autodesk.Revit.DB.Events;
using Bitmap = System.Drawing.Bitmap;
using BoundarySegment = Autodesk.Revit.DB.BoundarySegment;
using ComponentManager = Autodesk.Windows.ComponentManager;
using IWin32Window = System.Windows.Forms.IWin32Window;


namespace OpenCOVERPlugin
{

    public class COVERMessage
    {

        public MessageBuffer message;
        public int messageType;
        public COVERMessage(MessageBuffer m, int mt)
        {
            message = m;
            messageType = mt;
        }
    }

   public sealed class COVER
   {

       public enum MessageTypes { NewObject = 500, DeleteObject, ClearAll, UpdateObject, NewGroup, NewTransform, EndGroup, AddView, DeleteElement, NewParameters, SetParameter, NewMaterial, NewPolyMesh, NewInstance, EndInstance, SetTransform, UpdateView, AvatarPosition, RoomInfo };
      public enum ObjectTypes { Mesh = 1, Curve, Instance, Solid, RenderElement, Polymesh };
      private Thread messageThread;

      private System.Net.Sockets.TcpClient toCOVER;
      private Autodesk.Revit.DB.Options mOptions;
      private Autodesk.Revit.DB.View3D View3D;
      private Autodesk.Revit.DB.Document document;
      public Queue<COVERMessage> messageQueue;

      private EventHandler<Autodesk.Revit.UI.Events.IdlingEventArgs> idlingHandler;
      // DLL imports from user32.dll to set focus to
      // Revit to force it to forward the external event
      // Raise to actually call the external event 
      // Execute.

      /// <summary>
      /// The GetForegroundWindow function returns a 
      /// handle to the foreground window.
      /// </summary>
      [DllImport("user32.dll")]
      static extern IntPtr GetForegroundWindow();

      /// <summary>
      /// Move the window associated with the passed 
      /// handle to the front.
      /// </summary>
      [DllImport("user32.dll")]
      static extern bool SetForegroundWindow(
        IntPtr hWnd);   

      Autodesk.Revit.UI.ExternalEvent messageEvent;

      class externalMessageHandler : Autodesk.Revit.UI.IExternalEventHandler
      {

          /// <summary>
          /// Execute method invoked by Revit via the 
          /// external event as a reaction to a call 
          /// to its Raise method.
          /// </summary>
          public void Execute(Autodesk.Revit.UI.UIApplication a)
          {
              // As far as I can tell, the external event 
              // should work fine even when switching between
              // different documents. That, however, remains
              // to be tested in more depth (or at all).

              //Document doc = a.ActiveUIDocument.Document;

              //Debug.Assert( doc.Title.Equals( _doc.Title ),
              //  "oops ... different documents ... test this" );
              UIDocument uidoc = a.ActiveUIDocument;
              using (Autodesk.Revit.DB.Transaction transaction = new Autodesk.Revit.DB.Transaction(a.ActiveUIDocument.Document))
              {
                  FailureHandlingOptions failOpt = transaction.GetFailureHandlingOptions();

                  failOpt.SetClearAfterRollback(true);
                  failOpt.SetFailuresPreprocessor(new NoWarningsAndErrors());
                  transaction.SetFailureHandlingOptions(failOpt);
                  if (transaction.Start("changeParameters") == Autodesk.Revit.DB.TransactionStatus.Started)
                  {
                      while (COVER.Instance.messageQueue.Count > 0)
                      {
                          COVERMessage m = COVER.Instance.messageQueue.Dequeue();
                          COVER.Instance.handleMessage(m.message, m.messageType, a.ActiveUIDocument.Document,uidoc);
                          if (Autodesk.Revit.DB.TransactionStatus.Committed != transaction.Commit())
                          {
                              // Autodesk.Revit.UI.TaskDialog.Show("Failure", "Transaction could not be committed");
                              //an error occured end resolution was cancled thus this change can't be committed.
                              // just ignore it and dont bug the user
                          }
                          return;
                      }
                  }
              }
          }
          /// <summary>
          /// Required IExternalEventHandler interface 
          /// method returning a descriptive name.
          /// </summary>
          public string GetName()
          {
              return string.Format("COVISE Message Event handler");
          }
      }
      externalMessageHandler handler;
      int callCounter;
      IntPtr ApplicationWindow;
      FamilyInstance avatarObject;

      COVER()
      {

         mOptions = new Autodesk.Revit.DB.Options();
         mOptions.DetailLevel = Autodesk.Revit.DB.ViewDetailLevel.Fine;

      }
      /// <summary>
      /// Singleton class which holds the connection to OpenCOVER, is used to communicate with OpenCOVER
      /// </summary>
      public static COVER Instance
      {
         get
         {
            return Nested.instance;
         }
      }
      private Level getLevel(Document document, double height)
      {
          int levelNumber = 0;
          Level lastLevel = null;
          FilteredElementCollector collector = new FilteredElementCollector(document);
          ICollection<Element> collection = collector.OfClass(typeof(Level)).ToElements();
          foreach (Element e in collection)
          {
              Level level = e as Level;

              if (null != level)
              {
                  // keep track of number of levels
                  levelNumber++;
                  if (lastLevel != null)
                  {
                      if (height < level.Elevation && height > lastLevel.Elevation)
                      {
                          return lastLevel;
                      }
                  }
                  lastLevel = level;
              }
          }
          return null;
      }

      private Room getRoom(Document document, double x, double y, double height)
      {
          FilteredElementCollector a = new FilteredElementCollector(document).OfClass(typeof(SpatialElement));

          foreach (SpatialElement e in a)
          {
              Room room = e as Room;

              if (null != room)
              {
                  BoundingBoxXYZ bb = room.get_BoundingBox(null);
                  if ((bb.Min.X < x && x < bb.Max.X) && (bb.Min.Y < y && y < bb.Max.Y) && (bb.Min.Z < height && height < bb.Max.Z))
                  {
                      /* SpatialElementBoundaryOptions options = new SpatialElementBoundaryOptions();
                       options.SpatialElementBoundaryLocation = SpatialElementBoundaryLocation.Finish;
                       foreach (IList<Autodesk.Revit.DB.BoundarySegment> boundSegList in room.GetBoundarySegments(options))
                       {
                           boundSegList.
                           foreach (Autodesk.Revit.DB.BoundarySegment boundSeg in boundSegList)
                           {
                               Element e = boundSeg.Element;
                               Wall wall = e as Wall;
                               LocationCurve locationCurve = wall.Location as LocationCurve;
                               Curve curve = locationCurve.Curve;
                               roomElementInfo += e.Name + " " + curve.Length + "\n";
                           }
                       }*/
                      return room;
                  }
              }
          }
          return null;
      }
      public static uint SwapUnsignedInt(uint source)
      {
         return (uint)((((source & 0x000000FF) << 24)
         | ((source & 0x0000FF00) << 8)
         | ((source & 0x00FF0000) >> 8)
         | ((source & 0xFF000000) >> 24)));
      }

      public void SendGeometry(Autodesk.Revit.DB.FilteredElementIterator iter, Autodesk.Revit.DB.Document doc)
      {
         MessageBuffer mb = new MessageBuffer();
         mb.add(1);
         sendMessage(mb.buf, MessageTypes.ClearAll);
          // todo use the current or default view
         iter.Reset();
         while (iter.MoveNext())
         {
            if (iter.Current is Autodesk.Revit.DB.View3D)
            {
                View3D = iter.Current as Autodesk.Revit.DB.View3D;
              break;
            }
            // this one handles Group.
         }
          ElementId activeOptId = Autodesk.Revit.DB.DesignOption.GetActiveDesignOptionId(doc);
         iter.Reset();
         while (iter.MoveNext())
         {
             Autodesk.Revit.DB.Element el = iter.Current as Autodesk.Revit.DB.Element;
             if (el.DesignOption == null || el.DesignOption.Id == activeOptId)
             {
                 SendElement(el);
             }
            // this one handles Group.
         }
      }
      public void SendTypeParameters(Autodesk.Revit.DB.FilteredElementIterator iter)
      {
          iter.Reset();
          while (iter.MoveNext())
          {
              sendParameters(iter.Current as Autodesk.Revit.DB.Element);
          }
      }
      public void deleteElement(Autodesk.Revit.DB.ElementId ID)
      {
          MessageBuffer mb = new MessageBuffer();
          mb.add(ID.IntegerValue);
          sendMessage(mb.buf, MessageTypes.DeleteElement);
      }

      public void SendGeometry(Autodesk.Revit.DB.FilteredElementIterator iter, List<int> IDs)
      {
         iter.Reset();
         while (iter.MoveNext())
         {
            if (iter.Current is Autodesk.Revit.DB.View3D)
            {
               View3D = iter.Current as Autodesk.Revit.DB.View3D;
               break;
            }
            // this one handles Group.
         }
         iter.Reset();
         while (iter.MoveNext())
         {
            Autodesk.Revit.DB.Element elem;
            elem = iter.Current as Autodesk.Revit.DB.Element;
            foreach (int ID in IDs)   // get the wall type by the name
            {
                if (ID == elem.Id.IntegerValue)
               {
                  MessageBuffer mb = new MessageBuffer();
                  mb.add(ID);
                  sendMessage(mb.buf, MessageTypes.DeleteObject);
                  SendElement(elem);
                  break;
               }
            }
            // this one handles Group.
         }
      }

       public void sendFamilySymbolParameters(Autodesk.Revit.DB.Element elem)
      {
          // iterate element's parameters
          Autodesk.Revit.DB.ParameterSet vrps=new Autodesk.Revit.DB.ParameterSet();
          foreach (Autodesk.Revit.DB.Parameter para in elem.Parameters)
          {
              if (para.Definition.Name != null && para.Definition.Name.Length > 4)
              {
                  if (String.Compare(para.Definition.Name, 0, "coVR", 0, 4, true) == 0)
                  {
                      vrps.Insert(para);
                  }
              }
          }
          if (vrps.Size > 0)
          {
              MessageBuffer mb = new MessageBuffer();
              mb.add(elem.Id.IntegerValue);
              mb.add(elem.Name + "_FamilySymbol");
              mb.add((int)ObjectTypes.Mesh);
              mb.add(false);
              mb.add(0);

              int i = 0;


              mb.add((byte)220); // color
              mb.add((byte)220);
              mb.add((byte)220);
              mb.add((byte)255);
              mb.add(-1); // material ID
              sendMessage(mb.buf, MessageTypes.NewObject);

              mb = new MessageBuffer();
              mb.add(elem.Id.IntegerValue);
              mb.add(vrps.Size);
              foreach (Autodesk.Revit.DB.Parameter para in vrps)
              {
                  mb.add(para.Id.IntegerValue);
                  mb.add(para.Definition.Name);
                  mb.add((int)para.StorageType);
                  mb.add((int)para.Definition.ParameterType);
                  switch (para.StorageType)
                  {
                      case Autodesk.Revit.DB.StorageType.Double:
                          mb.add(para.AsDouble());
                          break;
                      case Autodesk.Revit.DB.StorageType.ElementId:
                          //find out the name of the element
                          Autodesk.Revit.DB.ElementId id = para.AsElementId();
                          mb.add(id.IntegerValue);
                          break;
                      case Autodesk.Revit.DB.StorageType.Integer:
                          mb.add(para.AsInteger());
                          break;
                      case Autodesk.Revit.DB.StorageType.String:
                          mb.add(para.AsString());
                          break;
                      default:
                          mb.add("Unknown Parameter Storage Type");
                          break;
                  }

              }
              sendMessage(mb.buf, MessageTypes.NewParameters);
          }
      }

       

      public void sendParameters(Autodesk.Revit.DB.Element elem)
      {
          // iterate element's parameters
          Autodesk.Revit.DB.ParameterSet vrps=new Autodesk.Revit.DB.ParameterSet();
          foreach (Autodesk.Revit.DB.Parameter para in elem.Parameters)
          {
              if (para.Definition.Name != null && para.Definition.Name.Length > 4)
              {
                  if (String.Compare(para.Definition.Name, 0, "coVR", 0, 4, true) == 0)
                  {
                      vrps.Insert(para);
                  }
              }
          }
          if (vrps.Size > 0)
          {

              MessageBuffer mb = new MessageBuffer();
              mb.add(elem.Id.IntegerValue);
              mb.add(vrps.Size);
              foreach (Autodesk.Revit.DB.Parameter para in vrps)
              {
                  mb.add(para.Id.IntegerValue);
                  mb.add(para.Definition.Name);
                  mb.add((int)para.StorageType);
                  mb.add((int)para.Definition.ParameterType);
                  switch (para.StorageType)
                  {
                      case Autodesk.Revit.DB.StorageType.Double:
                          mb.add(para.AsDouble());
                          break;
                      case Autodesk.Revit.DB.StorageType.ElementId:
                          //find out the name of the element
                          Autodesk.Revit.DB.ElementId id = para.AsElementId();
                          mb.add(id.IntegerValue);
                          break;
                      case Autodesk.Revit.DB.StorageType.Integer:
                          mb.add(para.AsInteger());
                          break;
                      case Autodesk.Revit.DB.StorageType.String:
                          mb.add(para.AsString());
                          break;
                      default:
                          mb.add("Unknown Parameter Storage Type");
                          break;
                  }

              }
              sendMessage(mb.buf, MessageTypes.NewParameters);
          }
      }
      // Note: Some element does not expose geometry, for example, curtain wall and dimension.
      // In case of a curtain wall, try selecting a whole wall by a window/box instead of a single pick.
      // It will then select internal components and be able to display its geometry.
      //
      public void SendElement(Autodesk.Revit.DB.Element elem)
      {
         if (elem.GetType() ==  typeof(Autodesk.Revit.DB.Element))
         {
            return;
         }
         if (elem is Autodesk.Revit.DB.View)
         {
             sendViewpoint(elem);
         }
         // if it is a Group. we will need to look at its components.
         if (elem is Autodesk.Revit.DB.Group)
         {

             /* if we add this, the elements of the Group are duplicates
                Autodesk.Revit.DB.Group @group = (Autodesk.Revit.DB.Group)elem;
                Autodesk.Revit.DB.ElementArray members = @group.GetMemberIds;


                MessageBuffer mb = new MessageBuffer();
                mb.add(elem.Id.IntegerValue);
                mb.add(elem.Name);
                sendMessage(mb.buf, MessageTypes.NewGroup);
                foreach (Autodesk.Revit.DB.Element elm in members)
                {
                   SendElement(elm);
                }
                mb = new MessageBuffer();
                sendMessage(mb.buf, MessageTypes.EndGroup);*/
         }

         else
         {


            // not a group. look at the geom data.
             Autodesk.Revit.DB.GeometryElement geom = elem.get_Geometry(mOptions);
            if ((geom != null))
            {
               SendElement(geom, elem);
            }
         }


      }

      /// <summary>
      /// Draw geometry of element.
      /// </summary>
      /// <param name="elementGeom"></param>
      /// <remarks></remarks>
      private void SendElement(Autodesk.Revit.DB.GeometryElement elementGeom, Autodesk.Revit.DB.Element elem)
      {
         if (elementGeom == null)
         {
            return;
         }
         try
         {
            if (elem.IsHidden(View3D))
            {
               return;
            }
            if (!elem.Category.get_Visible(View3D as Autodesk.Revit.DB.View))
            {
               return;
            }
         }
         catch
         {
         }
         int num = 0;
         IEnumerator<Autodesk.Revit.DB.GeometryObject> Objects = elementGeom.GetEnumerator();
         while (Objects.MoveNext())
         {
             Autodesk.Revit.DB.GeometryObject geomObject = Objects.Current;
            if (geomObject.Visibility == Autodesk.Revit.DB.Visibility.Visible)
            {
               if ((geomObject is Autodesk.Revit.DB.Curve))
               {
                  //mb.add((int)ObjectTypes.Curve);
                  //SendCurve(geomObject);
               }
               else if ((elem is Autodesk.Revit.DB.SpatialElement))
               {
                   // don't show room volumes
               }
               else if ((geomObject is Autodesk.Revit.DB.GeometryInstance))
               {
                   /*if (elem.Category.Name != "{3}")
                   {
                      if (!elem.Category.get_Visible(View3D as Autodesk.Revit.DB.View))
                      {
                         return;
                      }
                   }*/

                   SendInstance(geomObject as Autodesk.Revit.DB.GeometryInstance, elem);
               }
               else if ((geomObject is Autodesk.Revit.DB.Mesh))
               {
                   MessageBuffer mb = new MessageBuffer();
                   mb.add(elem.Id.IntegerValue);
                   mb.add(elem.Name + "_m_" + num.ToString());
                   mb.add((int)ObjectTypes.Mesh);
                   Autodesk.Revit.DB.Mesh meshObj = geomObject as Autodesk.Revit.DB.Mesh;
                   SendMesh(meshObj, ref mb, true);// TODO get information on whether a mesh is twosided or not

                   Autodesk.Revit.DB.ElementId materialID;
                   materialID = meshObj.MaterialElementId;
                   Autodesk.Revit.DB.Material materialElement = elem.Document.GetElement(materialID) as Autodesk.Revit.DB.Material;
                   if (materialElement != null)
                   {
                       mb.add(materialElement.Color);
                       mb.add((byte)(((100 - (materialElement.Transparency)) / 100.0) * 255));
                       mb.add(materialID.IntegerValue); // material ID
                   }
                   else
                   {
                       mb.add((byte)250); // color
                       mb.add((byte)250);
                       mb.add((byte)250);
                       mb.add((byte)255);
                       mb.add(-1); // material ID
                   }
                   sendMessage(mb.buf, MessageTypes.NewObject);
                   if (num == 0)
                   sendParameters(elem);
               }
               else if ((geomObject is Autodesk.Revit.DB.Solid))
               {
                   MessageBuffer mb = new MessageBuffer();
                   mb.add(elem.Id.IntegerValue);
                   mb.add(elem.Name + "_s_" + num.ToString());
                   sendMessage(mb.buf, MessageTypes.NewGroup);
                   SendSolid((Autodesk.Revit.DB.Solid)geomObject, elem);
                   mb = new MessageBuffer();
                   sendMessage(mb.buf, MessageTypes.EndGroup);
                   if(num == 0)
                    sendParameters(elem);
               }
               num++;
            }
         }
      }

      private void sendViewpoint(Autodesk.Revit.DB.Element elem)
      {
         Autodesk.Revit.DB.View view = (Autodesk.Revit.DB.View)elem;
         if (view is Autodesk.Revit.DB.View3D)
         {
            Autodesk.Revit.DB.View3D v3d = (Autodesk.Revit.DB.View3D)view;
            MessageBuffer mb = new MessageBuffer();
            mb.add(elem.Id.IntegerValue);
            mb.add(elem.Name);
            mb.add(v3d.Origin);
            mb.add(v3d.ViewDirection);
            mb.add(v3d.UpDirection);
            sendMessage(mb.buf, MessageTypes.AddView);
         }
         else
         {
         }
      }

      private void SendInstance(Autodesk.Revit.DB.GeometryInstance geomInstance, Autodesk.Revit.DB.Element elem)
      {

         MessageBuffer mb = new MessageBuffer();
         mb.add(elem.Id.IntegerValue);
         mb.add(elem.Name + "__" + elem.UniqueId.ToString());
         mb.add(geomInstance.Transform.BasisX.Multiply(geomInstance.Transform.Scale));
         mb.add(geomInstance.Transform.BasisY.Multiply(geomInstance.Transform.Scale));
         mb.add(geomInstance.Transform.BasisZ.Multiply(geomInstance.Transform.Scale));
         mb.add(geomInstance.Transform.Origin);
         sendMessage(mb.buf, MessageTypes.NewTransform);
         //Autodesk.Revit.DB.GeometryElement ge = geomInstance.GetInstanceGeometry(geomInstance.Transform);
         Autodesk.Revit.DB.GeometryElement ge = geomInstance.GetSymbolGeometry();
         SendElement(ge, elem);

         mb = new MessageBuffer();
         sendMessage(mb.buf, MessageTypes.EndGroup);

      }
      private bool equalMaterial(ref Autodesk.Revit.DB.Material m1,Autodesk.Revit.DB.Material m2)
      {
         if (m1 == null && m2 == null)
         {
            return true;
         }
         if (m1 != null && m2 != null)
         {
             if (m1.Id.IntegerValue == m2.Id.IntegerValue)
            {
               return true;
            }
            return false;
         }
         return false;
      }
      private void SendSolid(Autodesk.Revit.DB.Solid geomSolid, Autodesk.Revit.DB.Element elem)
      {
         Autodesk.Revit.DB.Material m = null;
         bool sameMaterial=true;
         int triangles=0;
         bool twoSided=false;

         Autodesk.Revit.DB.FaceArray faces = geomSolid.Faces;
         if (faces.Size == 0)
         {
            return;
         }


         Autodesk.Revit.DB.WallType wallType = elem.Document.GetElement(elem.GetTypeId()) as Autodesk.Revit.DB.WallType; // get element type
         if (wallType != null)
         {
             if (wallType.Kind == Autodesk.Revit.DB.WallKind.Curtain)
            {
               //return; // don't display curtain walls, these are probably fassades with bars and Glazing
            }
         }
         Autodesk.Revit.DB.ElementId materialID;
         materialID = faces.get_Item(0).MaterialElementId;
         foreach (Autodesk.Revit.DB.Face face in faces)
         {
            if (m == null)
            {
                materialID = face.MaterialElementId;
                Autodesk.Revit.DB.Material materialElement = elem.Document.GetElement(face.MaterialElementId) as Autodesk.Revit.DB.Material;

                /* Autodesk.Revit.DB.ElementId appearanceID = materialElement.AppearanceAssetId;
                 Autodesk.Revit.DB.AppearanceAssetElement ae = elem.Document.GetElement(appearanceID) as Autodesk.Revit.DB.AppearanceAssetElement;
                 Autodesk.Revit.Utility.Asset asset = ae.GetRenderingAsset();
                 Autodesk.Revit.DB.ParameterSet ps = ae.Parameters;
                 for (int i = 0; i < asset.Size; i++)
                 {
                     Autodesk.Revit.Utility.AssetProperty ap = asset[i];
                     string pn = ap.Name;
                     string val = ap.ToString();

                     System.Collections.Generic.IList<string> props = ap.GetConnectedPropertiesNames();
                     foreach (string p in props)
                     {
                         string pName = p;
                     }
                 }
                 foreach (Autodesk.Revit.DB.Parameter p in ae.Parameters)
                 {
                     string pName = p.AsString();
                     string val = p.AsValueString();
                 }
                System.Collections.Generic.IList<Autodesk.Revit.Utility.AssetProperty> props2 = asset.GetAllConnectedProperties();*/
                m = materialElement;
               twoSided = face.IsTwoSided;
            }
            Autodesk.Revit.DB.Mesh geomMesh = face.Triangulate();
            if (geomMesh != null)
            {
               triangles += geomMesh.NumTriangles;
               if (materialID != face.MaterialElementId)
               {
                  sameMaterial = false;
                  break;
               }
            }
         }
         if (triangles == 0)
            return;
         if (sameMaterial)
         {
            MessageBuffer mb = new MessageBuffer();
            mb.add(elem.Id.IntegerValue);
            mb.add(elem.Name + "_combined");
            mb.add((int)ObjectTypes.Mesh);
            mb.add(twoSided);
            mb.add(triangles);

            int i = 0;

            foreach (Autodesk.Revit.DB.Face face in geomSolid.Faces)
            {
               Autodesk.Revit.DB.Mesh geomMesh = face.Triangulate();
               if (geomMesh != null)
               {
                  for (i = 0; i < geomMesh.NumTriangles; i++)
                  {
                     Autodesk.Revit.DB.MeshTriangle triangle = default(Autodesk.Revit.DB.MeshTriangle);
                     triangle = geomMesh.get_Triangle(i);
                     mb.add((float)triangle.get_Vertex(0).X);
                     mb.add((float)triangle.get_Vertex(0).Y);
                     mb.add((float)triangle.get_Vertex(0).Z);
                     mb.add((float)triangle.get_Vertex(1).X);
                     mb.add((float)triangle.get_Vertex(1).Y);
                     mb.add((float)triangle.get_Vertex(1).Z);
                     mb.add((float)triangle.get_Vertex(2).X);
                     mb.add((float)triangle.get_Vertex(2).Y);
                     mb.add((float)triangle.get_Vertex(2).Z);
                  }
               }
            }

            if (m == null)
            {
               mb.add((byte)220); // color
               mb.add((byte)220);
               mb.add((byte)220);
               mb.add((byte)255);
               mb.add(-1); // material ID
            }
            else
            {
               mb.add(m.Color);
               mb.add((byte)(((100 - (m.Transparency)) / 100.0) * 255));
               mb.add(m.Id.IntegerValue);
            }
            sendMessage(mb.buf, MessageTypes.NewObject);
         }
         else
         {
            int num = 0;
            foreach (Autodesk.Revit.DB.Face face in geomSolid.Faces)
            {
               Autodesk.Revit.DB.Mesh geomMesh = face.Triangulate();
               if (geomMesh != null)
               {
                  MessageBuffer mb = new MessageBuffer();
                  mb.add(elem.Id.IntegerValue);
                  mb.add(elem.Name + "_f_" + num.ToString());
                  mb.add((int)ObjectTypes.Mesh);

                  SendMesh(geomMesh, ref mb, face.IsTwoSided);
                  if (face.MaterialElementId == Autodesk.Revit.DB.ElementId.InvalidElementId)
                  {
                     mb.add((byte)220); // color
                     mb.add((byte)220);
                     mb.add((byte)220);
                     mb.add((byte)255);
                     mb.add(-1); // material ID
                  }
                  else
                  {
                      Autodesk.Revit.DB.Material materialElement = elem.Document.GetElement(face.MaterialElementId) as Autodesk.Revit.DB.Material;

                     mb.add(materialElement.Color);
                     mb.add((byte)(((100 - (materialElement.Transparency)) / 100.0) * 255));
                     mb.add(materialElement.Id.IntegerValue);
                  }
                  sendMessage(mb.buf, MessageTypes.NewObject);
               }
               num++;
            }
         }

         //Autodesk.Revit.DB.Edge Edge = default(Autodesk.Revit.DB.Edge);
         //foreach (var Edge in geomSolid.Edges)
         //{
         //   DrawEdge(Edge);
         //}

      }
      /// <summary>
      /// send a mesh to OpenCOVER.
      /// </summary>
      /// <param name="geomMesh"></param>
      /// <remarks></remarks>
      private void SendMesh(Autodesk.Revit.DB.Mesh geomMesh, ref MessageBuffer mb, bool twoSided)
      {

         int i = 0;
         mb.add(twoSided);
         mb.add(geomMesh.NumTriangles);

         for (i = 0; i < geomMesh.NumTriangles; i++)
         {
            Autodesk.Revit.DB.MeshTriangle triangle = default(Autodesk.Revit.DB.MeshTriangle);
            triangle = geomMesh.get_Triangle(i);
            mb.add((float)triangle.get_Vertex(0).X);
            mb.add((float)triangle.get_Vertex(0).Y);
            mb.add((float)triangle.get_Vertex(0).Z);
            mb.add((float)triangle.get_Vertex(1).X);
            mb.add((float)triangle.get_Vertex(1).Y);
            mb.add((float)triangle.get_Vertex(1).Z);
            mb.add((float)triangle.get_Vertex(2).X);
            mb.add((float)triangle.get_Vertex(2).Y);
            mb.add((float)triangle.get_Vertex(2).Z);
         }


      }

      public static Element FindElementByName(
   Document doc,
   Type targetType,
   string targetName)
      {
          return new FilteredElementCollector(doc)
            .OfClass(targetType)
            .FirstOrDefault<Element>(
              e => e.Name.Equals(targetName));
      }

void TestAllOverloads(
  Document doc,
  XYZ startPoint,
  XYZ endPoint,
  FamilySymbol familySymbol )
{
  StructuralType stNon = StructuralType.NonStructural;
  StructuralType stBeam = StructuralType.Beam;
 
  Autodesk.Revit.Creation.Document cd
    = doc.Create;
 
  View view = doc.ActiveView;
  SketchPlane sk = view.SketchPlane;

  Level level = doc.GetElement(view.LevelId) as Level;
 
  // Create line from user points

  Curve curve = Line.CreateBound(startPoint, endPoint);
 
  // Create direction vector from user points
 
  XYZ dirVec = endPoint - startPoint;
 
  bool done = false;
  int index = 1;
  while( !done )
  {
    FamilyInstance instance = null;
 
    // Try different insert methods
 
    try
    {
      switch( index )
      {
        // public FamilyInstance NewFamilyInstance( 
        //   XYZ location, FamilySymbol symbol, 
        //   StructuralType structuralType );
 
        case 1:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, stNon );
          break;
 
        case 2:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, stBeam );
          break;
 
        // public FamilyInstance NewFamilyInstance( 
        //   XYZ origin, FamilySymbol symbol, 
        //   View specView );
 
        case 3:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, null );
          break;
 
        case 4:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, view );
          break;
 
        // public FamilyInstance NewFamilyInstance(
        //   XYZ location, FamilySymbol symbol, 
        //   Element host, StructuralType structuralType );
 
        case 5:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, sk, stNon );
          break;
 
        case 6:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, sk, stBeam );
          break;
 
        // public FamilyInstance NewFamilyInstance(
        //   XYZ location, FamilySymbol symbol, 
        //   XYZ referenceDirection, Element host, 
        //   StructuralType structuralType );
 
        case 7:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, dirVec, sk, 
            stNon );
          break;
 
        case 8:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, dirVec, sk, 
            stBeam );
          break;
 
        // public FamilyInstance NewFamilyInstance(
        //   Curve curve, FamilySymbol symbol, 
        //   Level level, StructuralType structuralType );
 
        case 9:
          instance = cd.NewFamilyInstance( 
            curve, familySymbol, null, stNon );
          break;
 
        case 10:
          instance = cd.NewFamilyInstance( 
            curve, familySymbol, null, stBeam );
          break;
 
        case 11:
          instance = cd.NewFamilyInstance( 
            curve, familySymbol, level, stNon );
          break;
 
        case 12:
          instance = cd.NewFamilyInstance( 
            curve, familySymbol, level, stBeam );
          break;
 
        // public FamilyInstance NewFamilyInstance(
        //   XYZ location, FamilySymbol symbol, 
        //   Level level, StructuralType structuralType );
 
        case 13:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, null, stNon );
          break;
 
        case 14:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, null, stBeam );
          break;
 
        case 15:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, level, stNon );
          break;
 
        case 16:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, level, stBeam );
          break;
 
        // public FamilyInstance NewFamilyInstance(
        //   XYZ location, FamilySymbol symbol, 
        //   Element host, Level level, 
        //   StructuralType structuralType );
 
        case 17:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, null, stNon );
          break;
 
        case 18:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, null, stBeam );
          break;
 
        case 19:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, sk, stNon );
          break;
 
        case 20:
          instance = cd.NewFamilyInstance( 
            startPoint, familySymbol, sk, stBeam );
          break;
 
        default:
          done = true;
          break;
      }
    }
    catch
    { }
 
    // If instance was created, mark with identifier so I can see which instances were created
 
  /*  if( null != instance )
    {
      Parameter param = instance.get_Parameter( "InstanceIndex" );
      if( null != param )
      {
        param.Set( index );
      }
    }*/
    index++;
  }
}
       
              List<ElementId> _added_element_ids = new List<ElementId>();
    void OnDocumentChanged(
    object sender,
    DocumentChangedEventArgs e )
  {
    _added_element_ids.AddRange(
      e.GetAddedElementIds() );
  }
      void createAvatar(Document doc,UIDocument uidoc)
      {
          string FamilyName = "RPC Mann";
          Family family = FindElementByName(doc, typeof(Family), FamilyName) as Family;
          if (null == family)
          {

              string libraryPath = "";
              doc.Application.GetLibraryPaths().TryGetValue("Metric Library", out libraryPath);

              //string _family_folder = libraryPath + "/Außenbauteile/RPC 3D-Objekte/";
              string _family_folder = libraryPath + "";

              //string _family_path = _family_folder + "/RPC Mann.rfa";
              string _family_path = _family_folder + "avatar.rfa";




              doc.LoadFamily(_family_path, out family);

          }
              // Determine the family symbol

              FamilySymbol familySymbol = null;
              Material material = null;

              ISet<ElementId> familySymbolIds = family.GetFamilySymbolIds();
              if (familySymbolIds.Count == 0)
              {
              }
              else
              {

                  // Get family symbols which is contained in this family
                  foreach (ElementId id in familySymbolIds)
                  {
                      familySymbol = family.Document.GetElement(id) as FamilySymbol;
                      // Get family symbol name
                      foreach (ElementId materialId in familySymbol.GetMaterialIds(false))
                      {
                          material = familySymbol.Document.GetElement(materialId) as Material;

                          break;
                      }
                      break;
                  }
              }
             /* XYZ newPos = new XYZ(0,0,0);
              int hosttype = family.get_Parameter(BuiltInParameter.FAMILY_HOSTING_BEHAVIOR).AsInteger();
              //       TestAllOverloads(doc, newPos, new XYZ(1, 0, 0), familySymbol);
              //       avatarObject = doc.Create.NewFamilyInstance(newPos, familySymbol, Autodesk.Revit.DB.Structure.StructuralType.NonStructural);  

              doc.Application.DocumentChanged
          += new EventHandler<Autodesk.Revit.DB.Events.DocumentChangedEventArgs>(
            OnDocumentChanged);

              _added_element_ids.Clear();

              // PromptForFamilyInstancePlacement cannot 
              // be called inside transaction.

              uidoc.PromptForFamilyInstancePlacement(familySymbol);

              doc.Application.DocumentChanged
                -= new EventHandler<DocumentChangedEventArgs>(
                  OnDocumentChanged);

              // Access the newly placed family instances:

              int n = _added_element_ids.Count();

              avatarObject = doc.GetElement(_added_element_ids[0]) as FamilyInstance;*/

              avatarObject = FindElementByName(doc, typeof(FamilyInstance), "Avatar") as FamilyInstance;
 
      }


      void handleMessage(MessageBuffer buf, int msgType,Document doc,UIDocument uidoc)
      {

          // create Avatar object if not present
         /* if (avatarObject == null)
          {
              createAvatar(doc,uidoc);
          }*/
          switch ((MessageTypes)msgType)
          {
              case MessageTypes.SetParameter:
                  {
                      int elemID = buf.readInt();
                      int paramID = buf.readInt();

                      Autodesk.Revit.DB.ElementId id = new Autodesk.Revit.DB.ElementId(elemID);
                      Autodesk.Revit.DB.Element elem = document.GetElement(id);

                      foreach (Autodesk.Revit.DB.Parameter para in elem.Parameters)
                      {
                          if (para.Id.IntegerValue == paramID)
                          {

                              switch (para.StorageType)
                              {
                                  case Autodesk.Revit.DB.StorageType.Double:
                                      double d = buf.readDouble();
                                      try
                                      {
                                          para.Set(d);
                                      }
                                      catch
                                      {
                                          Autodesk.Revit.UI.TaskDialog.Show("Double", "para.Set failed");
                                      }
                                      d = para.AsDouble();
                                      break;
                                  case Autodesk.Revit.DB.StorageType.ElementId:
                                      //find out the name of the element
                                      int tmpid = buf.readInt();
                                      Autodesk.Revit.DB.ElementId eleId = new Autodesk.Revit.DB.ElementId(tmpid);
                                      try
                                      {
                                          para.Set(eleId);
                                      }
                                      catch
                                      {
                                          Autodesk.Revit.UI.TaskDialog.Show("Double", "para.Set failed");
                                      }
                                      break;
                                  case Autodesk.Revit.DB.StorageType.Integer:
                                      try
                                      {
                                          para.Set(buf.readInt());
                                      }
                                      catch
                                      {
                                          Autodesk.Revit.UI.TaskDialog.Show("Double", "para.Set failed");
                                      }
                                      break;
                                  case Autodesk.Revit.DB.StorageType.String:
                                      try
                                      {
                                          para.Set(buf.readString());
                                      }
                                      catch
                                      {
                                          Autodesk.Revit.UI.TaskDialog.Show("Double", "para.Set failed");
                                      }
                                      break;
                                  default:
                                      try
                                      {
                                          para.SetValueString(buf.readString());
                                      }
                                      catch
                                      {
                                          Autodesk.Revit.UI.TaskDialog.Show("Double", "para.Set failed");
                                      }
                                      break;
                              }

                          }
                      }
                  }
                  break;
              case MessageTypes.SetTransform:
                  {
                      int elemID = buf.readInt();
                      double x = buf.readDouble();
                      double y = buf.readDouble();
                      double z = buf.readDouble();

                      Autodesk.Revit.DB.ElementId id = new Autodesk.Revit.DB.ElementId(elemID);
                      Autodesk.Revit.DB.Element elem = document.GetElement(id);
                      Autodesk.Revit.DB.XYZ translationVec = new Autodesk.Revit.DB.XYZ(x, y, z);
                      Autodesk.Revit.DB.LocationCurve ElementPosCurve = elem.Location as Autodesk.Revit.DB.LocationCurve;
                      if (ElementPosCurve != null)
                          ElementPosCurve.Move(translationVec);
                      Autodesk.Revit.DB.LocationPoint ElementPosPoint = elem.Location as Autodesk.Revit.DB.LocationPoint;
                      if (ElementPosPoint != null)
                          ElementPosPoint.Move(translationVec);
                  }
                  break;
              case MessageTypes.UpdateView:
                  {
                      int elemID = buf.readInt();
                      double ex = buf.readDouble();
                      double ey = buf.readDouble();
                      double ez = buf.readDouble();
                      double dx = buf.readDouble();
                      double dy = buf.readDouble();
                      double dz = buf.readDouble();
                      double ux = buf.readDouble();
                      double uy = buf.readDouble();
                      double uz = buf.readDouble();

                      Autodesk.Revit.DB.ElementId id = new Autodesk.Revit.DB.ElementId(elemID);
                      Autodesk.Revit.DB.Element elem = document.GetElement(id);
                      Autodesk.Revit.DB.View3D v3d = (Autodesk.Revit.DB.View3D)elem;
                      Autodesk.Revit.DB.ViewOrientation3D ori = new Autodesk.Revit.DB.ViewOrientation3D(new Autodesk.Revit.DB.XYZ(ex, ey, ez), new Autodesk.Revit.DB.XYZ(ux, uy, uz), new Autodesk.Revit.DB.XYZ(dx, dy, dz));
                      v3d.SetOrientation(ori);
                  }
                  break;

              case MessageTypes.AvatarPosition:
                  {
                      double ex = buf.readDouble();
                      double ey = buf.readDouble();
                      double ez = buf.readDouble();
                      double dx = buf.readDouble();
                      double dy = buf.readDouble();
                      double dz = buf.readDouble();
                      Level currentLevel = getLevel(doc, ez);
                      string lev = "";
                      if (currentLevel != null)
                      {
                          lev = currentLevel.Name;
                      }
                      Room testRaum = getRoom(doc, ex, ey, ez);
                      XYZ point = new XYZ(ex, ey, ez);
                      Room currentRoom = doc.GetRoomAtPoint(point);
                      if (currentRoom == null)
                      {
                          point = new XYZ(ex, ey, currentLevel.ProjectElevation);
                          currentRoom = doc.GetRoomAtPoint(point);
                          if (currentRoom == null)
                              currentRoom = testRaum;
                      }
                      if (currentRoom != null)
                      {
                          
                          string nr = currentRoom.Number;
                          string name = currentRoom.Name;
                          double area = currentRoom.Area;
                          MessageBuffer mb = new MessageBuffer();
                          mb.add(nr);
                          mb.add(name);
                          mb.add(area);
                          mb.add(lev);
                          sendMessage(mb.buf, MessageTypes.RoomInfo);
                      }
                      else
                      {
                          string nr = "-1";
                          string name = "No Room";
                          double area = 0.0;
                          MessageBuffer mb = new MessageBuffer();
                          mb.add(nr);
                          mb.add(name);
                          mb.add(area);
                          mb.add(lev);
                          sendMessage(mb.buf, MessageTypes.RoomInfo);
                      }
                      if (avatarObject != null)
                      {
                          /*
                          Autodesk.Revit.DB.LocationCurve ElementPosCurve = avatarObject.Location as Autodesk.Revit.DB.LocationCurve;
                          Autodesk.Revit.DB.XYZ translationVec = new Autodesk.Revit.DB.XYZ(ex, ey, ez);
                          ElementPosCurve.Move(translationVec);*/
                      }
                  }
                  break;
                  
          }
      }

      public void handleMessages()
      {
          if (toCOVER != null)
          {
              Byte[] data = new Byte[2000];
              while (true)
              {
                  int len = 0;
                  while (len < 16)
                  {
                      int numRead;
                      try
                      {
                          numRead = toCOVER.GetStream().Read(data, len, 16 - len);
                      }
                      catch (System.IO.IOException e)
                      {
                          // probably socket closed
                          return;
                      }
                      len += numRead;
                  }

                  int msgType = BitConverter.ToInt32(data, 2 * 4);
                  int length = BitConverter.ToInt32(data, 3 * 4);
                  length = (int)ByteSwap.swap((uint)length);
                  msgType = (int)ByteSwap.swap((uint)msgType);
                  len = 0;
                  while (len < length)
                  {
                      int numRead;
                      try
                      {
                          numRead = toCOVER.GetStream().Read(data, len, length - len);
                      }
                      catch (System.IO.IOException e)
                      {
                          // probably socket closed
                          return;
                      }
                      len += numRead;
                  }
                  COVERMessage m = new COVERMessage(new MessageBuffer(data), msgType);
                  messageQueue.Enqueue(m);

                  messageEvent.Raise();
                  IntPtr hBefore = GetForegroundWindow();

                  SetForegroundWindow(Autodesk.Windows.ComponentManager.ApplicationWindow);

                  SetForegroundWindow(hBefore);
              }
          }
      }
      public void startup(UIControlledApplication application)
      {
          idlingHandler = new EventHandler<Autodesk.Revit.UI.Events.IdlingEventArgs>(idleUpdate);
          application.Idling += idlingHandler;
      }
      public void shutdown(UIControlledApplication application)
      {
          application.Idling -= idlingHandler;
      }
      public class NoWarningsAndErrors : IFailuresPreprocessor
      {
          public FailureProcessingResult PreprocessFailures(
            FailuresAccessor a)
          {
              // inside event handler, get all warnings

              //IList<FailureMessageAccessor> failures
              //   = a.GetFailureMessages();
              a.DeleteAllWarnings();
              /*foreach (FailureMessageAccessor f in failures)
              {
                  // check failure definition ids 
                  // against ones to dismiss:

                  FailureDefinitionId id
                    = f.GetFailureDefinitionId();

                  if (BuiltInFailures.RoomFailures.RoomNotEnclosed
                    == id)
                  {
                      a.DeleteWarning(f);
                  }
              }*/

              IList<FailureMessageAccessor> failures = a.GetFailureMessages();
              if (failures.Count > 0)
                  return FailureProcessingResult.ProceedWithRollBack;
              else
                  return FailureProcessingResult.Continue;
          }
      }
      public void idleUpdate(object sender, Autodesk.Revit.UI.Events.IdlingEventArgs e)
      {
          e.SetRaiseWithoutDelay();

          UIApplication uiapp = sender as UIApplication;
          Document doc = uiapp.ActiveUIDocument.Document;
          UIDocument uidoc = uiapp.ActiveUIDocument;

          if (COVER.Instance.messageQueue.Count > 0)
          {
              using (Autodesk.Revit.DB.Transaction transaction = new Autodesk.Revit.DB.Transaction(doc))
              {
                  FailureHandlingOptions failOpt = transaction.GetFailureHandlingOptions();

                  failOpt.SetClearAfterRollback(true);
                  failOpt.SetFailuresPreprocessor( new NoWarningsAndErrors());
                  transaction.SetFailureHandlingOptions(failOpt);

                  if (transaction.Start("changeParameters") == Autodesk.Revit.DB.TransactionStatus.Started)
                  {
                      while (COVER.Instance.messageQueue.Count > 0)
                      {
                          COVERMessage m = COVER.Instance.messageQueue.Dequeue();
                          COVER.Instance.handleMessage(m.message, m.messageType,doc,uidoc);
                          if (Autodesk.Revit.DB.TransactionStatus.Committed != transaction.Commit())
                          {
                              // Autodesk.Revit.UI.TaskDialog.Show("Failure", "Transaction could not be committed");
                              //an error occured end resolution was cancled thus this change can't be committed.
                              // just ignore it and dont bug the user
                          }
                          return;
                      }
                  }
              }
          }
      }

      public bool ConnectToOpenCOVER(string host, int port, Autodesk.Revit.DB.Document doc)
      {
         document = doc;
         handler = new externalMessageHandler();
         messageEvent = Autodesk.Revit.UI.ExternalEvent.Create(handler);
         messageQueue = new Queue<COVERMessage>();

         System.Diagnostics.Process[] processes = System.Diagnostics.Process.GetProcessesByName("Revit");
         callCounter = 0;

         if (0 < processes.Length)
         {
             ApplicationWindow = processes[0].MainWindowHandle;
         }
         try
         {
             if (toCOVER != null)
             {
                 messageThread.Abort(); // stop reading from the old toCOVER connection
                 toCOVER.Close();
                 toCOVER = null;
             }

             toCOVER = new TcpClient(host, port);
             if (toCOVER.Connected)
             {
                 // Sends data immediately upon calling NetworkStream.Write.
                 toCOVER.NoDelay = true;
                 LingerOption lingerOption = new LingerOption(false, 0);
                 toCOVER.LingerState = lingerOption;

                 NetworkStream s = toCOVER.GetStream();
                 Byte[] data = new Byte[256];
                 data[0] = 1;
                 try
                 {
                     //toCOVER.ReceiveTimeout = 1000;
                     s.Write(data, 0, 1);
                     //toCOVER.ReceiveTimeout = 10000;
                 }
                 catch (System.IO.IOException e)
                 {
                     // probably socket closed
                     return false;
                 }

                 int numRead = 0;
                 try
                 {
                     //toCOVER.ReceiveTimeout = 1000;
                     numRead = s.Read(data, 0, 1);
                     //toCOVER.ReceiveTimeout = 10000;
                 }
                 catch (System.IO.IOException e)
                 {
                     // probably socket closed
                     return false;
                 }
                 if (numRead == 1)
                 {

                     messageThread = new Thread(new ThreadStart(this.handleMessages));

                     // Start the thread
                     messageThread.Start();
                 }

                 return true;
             }
             System.Windows.Forms.MessageBox.Show("Could not connect to OpenCOVER on localhost, port 31821");
         }
         catch
         {
             System.Windows.Forms.MessageBox.Show("Connection error while trying to connect to OpenCOVER on localhost, port 31821");
         }
         return false;

      }

      public void sendMessage(Byte[] messageData, OpenCOVERPlugin.COVER.MessageTypes msgType)
      {
         int len = messageData.Length + (4 * 4);
         Byte[] data = new Byte[len];
         ByteSwap.swapTo((uint)msgType, data, 2 * 4);
         ByteSwap.swapTo((uint)messageData.Length, data, 3 * 4);
         messageData.CopyTo(data, 4 * 4);
         toCOVER.GetStream().Write(data, 0, len);
      }

      class Nested
      {
         // Explicit static constructor to tell C# compiler
         // not to mark type as beforefieldinit
         static Nested()
         {
         }
         internal static readonly COVER instance = new COVER();
      }
   }

}

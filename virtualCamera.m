tP = [-1000; -1200; 3500];

theta = [20 190 -60];
theta = deg2rad(theta);
%theta = [58.5190    3.2672 -160.5783];
rotX = [1 0 0; 0 cos(theta(1)) -sin(theta(1)); 0 sin(theta(1)) cos(theta(1))];
rotY = [cos(theta(2)) 0 sin(theta(2)); 0 1 0; -sin(theta(2)) 0 cos(theta(2))];
rotZ = [cos(theta(3)) -sin(theta(3)) 0; sin(theta(3)) cos(theta(3)) 0; 0 0 1];
rotMP = rotZ * rotY * rotX;

rotM = rotMP';
t = (0 - tP);
t = rotM * t;

focal = 2000;
principalPoint = [0; 0];

%fieldPoints = [4490,-4490,-4490,4490,4490,-4490,0,0; 2995,2995,-2995,-2995,0,0,2995,-2995; 0,0,0,0,0,0,0,0];
fieldPoints = [4490,-4490,-4490,4490,4490,-4490,0,0,4490,0; 2995,2995,-2995,-2995,0,0,2995,-2995,0,0; 0,0,0,0,0,0,0,0,0,0];
nPoints = size(fieldPoints);
nPoints = nPoints(2);

t = repmat(t, 1, nPoints);
imagePoints = rotM * fieldPoints;
imagePoints = imagePoints + t;
cameraPoints = imagePoints;

figure;
axis image
x = [cameraPoints(1,1),cameraPoints(1,2),cameraPoints(1,3),cameraPoints(1,4),cameraPoints(1,5),cameraPoints(1,10),cameraPoints(1,7),cameraPoints(1,10);
     cameraPoints(1,2),cameraPoints(1,3),cameraPoints(1,4),cameraPoints(1,1),cameraPoints(1,10),cameraPoints(1,6),cameraPoints(1,10),cameraPoints(1,8)];
y = [cameraPoints(2,1),cameraPoints(2,2),cameraPoints(2,3),cameraPoints(2,4),cameraPoints(2,5),cameraPoints(2,10),cameraPoints(2,7),cameraPoints(2,10);
     cameraPoints(2,2),cameraPoints(2,3),cameraPoints(2,4),cameraPoints(2,1),cameraPoints(2,10),cameraPoints(2,6),cameraPoints(2,10),cameraPoints(2,8)];
z = [cameraPoints(3,1),cameraPoints(3,2),cameraPoints(3,3),cameraPoints(3,4),cameraPoints(3,5),cameraPoints(3,10),cameraPoints(3,7),cameraPoints(3,10);
     cameraPoints(3,2),cameraPoints(3,3),cameraPoints(3,4),cameraPoints(3,1),cameraPoints(3,10),cameraPoints(3,6),cameraPoints(3,10),cameraPoints(3,8)];
subplot(1,3,1);
plot3(x(:,1),y(:,1),z(:,1),'r', x(:,2),y(:,2),z(:,2),'g', x(:,3),y(:,3),z(:,3),'b',x(:,4),y(:,4),z(:,4),'k', x(:,5),y(:,5), z(:,5),'b--o', x(:,6),y(:,6), z(:,6),'b--o', x(:,7),y(:,7), z(:,7),'r--o', x(:,8),y(:,8), z(:,8),'r--o');

zMatrix = zeros(nPoints, nPoints);
for index=1:nPoints
    zMatrix(index,index) = imagePoints(3,index);
end
imagePoints = [focal 0;0 focal] * imagePoints(1:2,:) + repmat(principalPoint,1,nPoints) * zMatrix;
 for index=1:nPoints
    imagePoints(:,index) =  imagePoints(:,index) / zMatrix(index,index);
 end
subplot(1,3,2);

x = [imagePoints(1,1),imagePoints(1,2),imagePoints(1,3),imagePoints(1,4),imagePoints(1,5),imagePoints(1,10),imagePoints(1,7), imagePoints(1,10);
     imagePoints(1,2),imagePoints(1,3),imagePoints(1,4),imagePoints(1,1),imagePoints(1,10),imagePoints(1,6),imagePoints(1,10), imagePoints(1,8)];
y = [imagePoints(2,1),imagePoints(2,2),imagePoints(2,3),imagePoints(2,4),imagePoints(2,5),imagePoints(2,10),imagePoints(2,7), imagePoints(2,10);
     imagePoints(2,2),imagePoints(2,3),imagePoints(2,4),imagePoints(2,1),imagePoints(2,10),imagePoints(2,6),imagePoints(2,10), imagePoints(2,8)];
plot(x(:,1),y(:,1),'r', x(:,2),y(:,2),'g', x(:,3),y(:,3),'b',x(:,4),y(:,4),'k', x(:,5),y(:,5),'b--o', x(:,6),y(:,6),'b--o', x(:,7),y(:,7),'r--o', x(:,8),y(:,8),'r--o');
set(gca,'XAxisLocation','top','YAxisLocation','left','ydir','reverse');

y = [4490,-4490,-4490,4490,4490,0;-4490,-4490,4490,4490,-4490,0];
x = [2995,2995,-2995,-2995,0,2995;2995,-2995,-2995,2995,0,-2995];
subplot(1,3,3);
plot(x(:,1),y(:,1),'r', x(:,2),y(:,2),'g', x(:,3),y(:,3),'b',x(:,4),y(:,4),'k', x(:,5),y(:,5), 'b--o', x(:,6),y(:,6), 'r--o')
set(gca,'xdir', 'reverse');

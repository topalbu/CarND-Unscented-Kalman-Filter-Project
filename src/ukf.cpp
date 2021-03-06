#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
    // if this is false, laser measurements will be ignored (except during init)
    use_laser_ = true;

    // if this is false, radar measurements will be ignored (except during init)
    use_radar_ = true;

    // initialize previous_timestamp_
    previous_timestamp_ = 0;

    // initial state vector
    x_ = VectorXd(5);

    // initial covariance matrix
    P_ = MatrixXd(5, 5);

    // Process noise standard deviation longitudinal acceleration in m/s^2
    std_a_ = 30;

    //set augmented dimension
    n_aug_= 7;

    //set state dimension
    n_x_ = 5;

    // Process noise standard deviation yaw acceleration in rad/s^2
    std_yawdd_ = 30;

    // Laser measurement noise standard deviation position1 in m
    std_laspx_ = 0.15;

    // Laser measurement noise standard deviation position2 in m
    std_laspy_ = 0.15;

    // Radar measurement noise standard deviation radius in m
    std_radr_ = 0.3;

    // Radar measurement noise standard deviation angle in rad
    std_radphi_ = 0.03;

    // Radar measurement noise standard deviation radius change in m/s
    std_radrd_ = 0.3;

    /**
    TODO:

    Complete the initialization. See ukf.h for other member properties.

    Hint: one or more values initialized above might be wildly off...
    */
    //set measurement dimension, radar can measure r, phi, and r_dot
    n_z_radar_= 3;

    //set measurement dimension, laser can measure px, py
    n_z_laser_= 2;

    R_laser_ = MatrixXd(n_z_laser_, n_z_laser_);
    R_radar_ = MatrixXd(n_z_radar_, n_z_radar_);
    H_laser_ = MatrixXd(2, 5);

    //measurement covariance matrix - laser
    R_laser_ << 0.0225, 0,
            0, 0.0225;

    //measurement covariance matrix - radar
    R_radar_ << 0.09, 0, 0,
            0, 0.0009, 0,
            0, 0, 0.09;

    P_ << 1, 0, 0, 0, 0,
            0, 1, 0, 0, 0,
            0, 0, 10000, 0, 0,
            0, 0, 0, 1000, 0,
            0, 0, 0, 0, 1;

    //measurement matrix
    H_laser_ << 1, 0, 0, 0, 0,
            0, 1, 0, 0, 0;

    Xsig_aug_ = MatrixXd(n_aug_, 2 * n_aug_ + 1);

    Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);

    //create augmented mean vector
    x_aug_ = VectorXd(7);

    //create augmented state covariance
    P_aug_ = MatrixXd(7, 7);
    P_aug_.fill(0.0);
    P_aug_.topLeftCorner(5,5) = P_;
    P_aug_(5,5) = std_a_*std_a_;
    P_aug_(6,6) = std_yawdd_*std_yawdd_;

    //create vector for weights
    weights_ = VectorXd(2*n_aug_+1);
    double weight_0 = lambda_/(lambda_+n_aug_);
    weights_(0) = weight_0;
    for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights
        double weight = 0.5/(n_aug_+lambda_);
        weights_(i) = weight;
    }

    //create matrix for sigma points in measurement space
    Zsig_ = MatrixXd(n_z_radar_, 2 * n_aug_ + 1);
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
    /*****************************************************************************
     *  Initialization
     ****************************************************************************/
    if (!is_initialized_) {
        // first measurement
        cout << "UKF : " << endl;
        double px , py = 0.0;

        if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
            /**
            Convert radar from polar to cartesian coordinates and initialize state.
            */
            cout << "Kalman Filter Initialization RADAR " << endl;
            double rho = meas_package.raw_measurements_[0];
            double phi = meas_package.raw_measurements_[1];

            px = rho * cos(phi);
            py = rho * sin(phi);


        }
        else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
            /**
            Initialize state.
            */
            cout << "Kalman Filter Initialization LASER " << endl;

            //set the state with the initial location and zero velocity

            px = meas_package.raw_measurements_[0];
            py = meas_package.raw_measurements_[1];


        }

        // done initializing, no need to predict or update
        x_ << px, py, 0, 0, 0;
        x_aug_.head(5) = x_;
        x_aug_(5) = 0;
        x_aug_(6) = 0;
        previous_timestamp_ = meas_package.timestamp_;
        is_initialized_ = true;
        if(fabs(px) < 0.0001){
            px = 1;
            P_(0,0)= 1000;
        }
        if(fabs(py) < 0.0001){
            py = 1;
            P_(1,1)= 1000;
        }
        return;
    }


    //compute the time elapsed between the current and previous measurements
    float dt = (meas_package.timestamp_ - previous_timestamp_) / 1000000.0;	//dt - expressed in seconds
    previous_timestamp_ = meas_package.timestamp_;

    float dt_2 = dt * dt;
    float dt_3 = dt_2 * dt;
    float dt_4 = dt_3 * dt;



    Prediction(dt);

    if (meas_package.sensor_type_ == MeasurementPackage::RADAR ) {
        // Radar updates
        UpdateRadar(meas_package);
    } else {
        // Laser updates
        UpdateLidar(meas_package);
    }

    // print the output
    //cout << "x_ = " << x_ << endl;
    //cout << "P_ = " << P_ << endl;
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
    //create augmented mean state
    x_aug_.head(5) = x_;
    x_aug_(5) = 0;
    x_aug_(6) = 0;

    //create augmented covariance matrix
    P_aug_.topLeftCorner(5,5) = P_;
    P_aug_(5,5) = std_a_*std_a_;
    P_aug_(6,6) = std_yawdd_*std_yawdd_;

    //create square root matrix
    MatrixXd L = P_aug_.llt().matrixL();

    //create augmented sigma points
    Xsig_aug_.col(0)  = x_aug_;
    for (int i = 0; i< n_aug_; i++)
    {
        Xsig_aug_.col(i+1)       = x_aug_ + sqrt(lambda_+n_aug_) * L.col(i);
        Xsig_aug_.col(i+1+n_aug_) = x_aug_ - sqrt(lambda_+n_aug_) * L.col(i);
    }

    //predict sigma points
    for (int i = 0; i< 2*n_aug_+1; i++) {
        //extract values for better readability
        double p_x = Xsig_aug_(0,i);
        double p_y = Xsig_aug_(1, i);
        double v = Xsig_aug_(2, i);
        double yaw = Xsig_aug_(3, i);
        double yawd = Xsig_aug_(4, i);
        double nu_a = Xsig_aug_(5, i);
        double nu_yawdd = Xsig_aug_(6, i);

        //predicted state values
        double px_p, py_p;

        //avoid division by zero
        if (fabs(yawd) > 0.001) {
            px_p = p_x + v / yawd * (sin(yaw + yawd * delta_t) - sin(yaw));
            py_p = p_y + v / yawd * (cos(yaw) - cos(yaw + yawd * delta_t));
        } else {
            px_p = p_x + v * delta_t * cos(yaw);
            py_p = p_y + v * delta_t * sin(yaw);
        }

        double v_p = v;
        double yaw_p = yaw + yawd * delta_t;
        double yawd_p = yawd;

        //add noise
        px_p = px_p + 0.5 * nu_a * delta_t * delta_t * cos(yaw);
        py_p = py_p + 0.5 * nu_a * delta_t * delta_t * sin(yaw);
        v_p = v_p + nu_a * delta_t;

        yaw_p = yaw_p + 0.5 * nu_yawdd * delta_t * delta_t;
        yawd_p = yawd_p + nu_yawdd * delta_t;

        //write predicted sigma point into right column
        Xsig_pred_(0, i) = px_p;
        Xsig_pred_(1, i) = py_p;
        Xsig_pred_(2, i) = v_p;
        Xsig_pred_(3, i) = yaw_p;
        Xsig_pred_(4, i) = yawd_p;
    }

    //predicted state mean
    x_.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
        x_ = x_+ weights_(i) * Xsig_pred_.col(i);
    }

    //predicted state covariance matrix
    P_.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        //angle normalization
        while (x_diff(3) > M_PI) x_diff(3) -= 2. * M_PI;
        while (x_diff(3) < -M_PI) x_diff(3) += 2. * M_PI;

        P_ = P_ + weights_(i) * x_diff * x_diff.transpose();
    }
    //print result
//    std::cout << "Predicted state" << std::endl;
//    std::cout << x_ << std::endl;
//    std::cout << "Predicted covariance matrix" << std::endl;
//    std::cout << P_ << std::endl;
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
    VectorXd z  = meas_package.raw_measurements_;
    VectorXd z_pred = H_laser_ * x_;
    VectorXd y = z - z_pred;
    MatrixXd Ht = H_laser_.transpose();
    MatrixXd S = H_laser_ * P_ * Ht + R_laser_;
    MatrixXd Si = S.inverse();
    MatrixXd PHt = P_ * Ht;
    MatrixXd K = PHt * Si;

    //new estimate
    x_ = x_ + (K * y);
    long x_size = x_.size();
    MatrixXd I = MatrixXd::Identity(x_size, x_size);
    P_ = (I - K * H_laser_) * P_;

    NIS_laser_ = z.transpose() * S.inverse() * z;
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
    /**
    TODO:

    Complete this function! Use radar data to update the belief about the object's
    position. Modify the state vector, x_, and covariance, P_.

    You'll also need to calculate the radar NIS.
    */

    //transform sigma points into measurement space
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

        // extract values for better readibility
        double p_x = Xsig_pred_(0,i);
        double p_y = Xsig_pred_(1,i);
        double v  = Xsig_pred_(2,i);
        double yaw = Xsig_pred_(3,i);

        double v1 = cos(yaw)*v;
        double v2 = sin(yaw)*v;

        // measurement model
        double r = sqrt(p_x*p_x + p_y*p_y);                        //r
        double phi = atan2(p_y,p_x);                                 //phi
        double r_dot  = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot

        if (!isfinite(r)){
            printf ("r  : %f\n",r);
            r = 0;
        }

        if (!isfinite(phi)){
            printf ("phi  : %f\n",phi);
            phi = 0;
        }

        if (!isfinite(r_dot)){
            printf ("r_dot  : %f\n",r_dot);
            r_dot = 0;
        }
        Zsig_(0,i) = r;
        Zsig_(1,i) = phi;
        Zsig_(2,i) = r_dot;
    }

    //mean predicted measurement
    VectorXd z_pred = VectorXd(n_z_radar_);
    z_pred.fill(0.0);
    for (int i=0; i < 2*n_aug_+1; i++) {
        z_pred = z_pred + weights_(i) * Zsig_.col(i);
    }

    //measurement covariance matrix S
    MatrixXd S = MatrixXd(n_z_radar_,n_z_radar_);
    S.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
        //residual
        VectorXd z_diff = Zsig_.col(i) - z_pred;

        //angle normalization
        while (z_diff(1) > M_PI) z_diff(1) -= 2. * M_PI;
        while (z_diff(1) < -M_PI) z_diff(1) += 2. * M_PI;

        S = S + weights_(i) * z_diff * z_diff.transpose();
    }

    //add measurement noise covariance matrix
    S = S + R_radar_;

    //create matrix for cross correlation Tc
    MatrixXd Tc = MatrixXd(n_x_, n_z_radar_);

    //calculate cross correlation matrix
    Tc.fill(0.0);
    for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

        //residual
        VectorXd z_diff = Zsig_.col(i) - z_pred;

        //angle normalization
        while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
        while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

        // state difference
        VectorXd x_diff = Xsig_pred_.col(i) - x_;
        //angle normalization
        while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
        while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

        Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
        //std::cout << "x_diff * z_diff.transpose(): " << std::endl << x_diff * z_diff.transpose() << std::endl;
    }
    //std::cout << "Tc : " << std::endl <<Tc << std::endl;
    //std::cout << "weights(1) : " << std::endl << weights_(1) << std::endl;
    //Kalman gain K;
    MatrixXd K = Tc * S.inverse();

    //residual
    VectorXd z = meas_package.raw_measurements_;
    VectorXd z_diff = z - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    //update state mean and covariance matrix
    x_ = x_ + K * z_diff;
    P_ = P_ - K*S*K.transpose();
    NIS_radar_ = z.transpose() * S.inverse() * z;
}
